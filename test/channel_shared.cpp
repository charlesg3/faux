#include <assert.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <pthread.h>
#include <messaging/dispatch.h>
#include <common/tsc.h>
#include <common/util.h>
#include <core/core.h>

const int MAX_SERVERS = 64;
const int NUM_MSG = 10000000;

int argc;
int num_servers;
char ** argv;

Endpoint * server_ep;
Address server_addr;
Channel * server_ch;

class Barrier {
    int ctr;
    int gen;

public:
    Barrier()
    { ctr = gen = 0; }

    void wait(int n) {
        int current = gen;
        int b = __sync_add_and_fetch(&ctr, 1);

        if (b == n) {
            __sync_add_and_fetch(&gen, 1);
            ctr = 0;
        }

        while (gen == current) {
            __sync_synchronize();
        }
    }
};

struct Shared {
    pid_t servers[ MAX_SERVERS ];
    int served[ MAX_SERVERS ];
    pid_t client;
    double result;
    Barrier full;
    bool quit;
};
Shared * shared;

void server(int id) {
    int cpu;

    cpu = strtol(argv[2+id], NULL, 10);
    printf("[%d] server %d running on %d\n", getpid(), id, cpu);
    bind_to_core(cpu);

    shared->full.wait(num_servers + 1);

    wmb();

    // uint64_t begin = rdtsc64();
    shared->served[id] = 0;
    while (!shared->quit) {
        int rmsg;
        int * ri;
        __attribute__((unused)) size_t rs;
        int * si;

        do {
        rs = channel_receive(server_ch, (void**)&ri);
        }
        while (rs == 0 && !shared->quit);

        if (rs == 0) {
            break;
        }

        rmsg = *ri;
        assert(rs == sizeof(int));
        channel_free(server_ch, ri);

        si = (int*)channel_allocate(server_ch, sizeof(int));
        assert(si);
        *si = rmsg;
        channel_send(server_ch, si, sizeof(int));

        ++shared->served[id];
    }
    // uint64_t end = rdtsc64();

    printf("[%d] server %d served %d requests.\n", getpid(), id, shared->served[id]);

    shared->full.wait(num_servers + 1);

    // printf("[%d] %d -> %f\n", getpid(), id, ((double) (end - begin)) / NUM_MSG);

    if (id == 0) {
        channel_close(server_ch);
    }
}

void client(int id) {
    int cpu;

    assert(id == 0);            // single client for this test

    cpu = strtol(argv[1+id], NULL, 10);
    printf("[%d] client %d running on %d\n", getpid(), id, cpu);
    bind_to_core(cpu);

    Endpoint * ep = endpoint_create();

    Channel * ch;
    do {
        ch = endpoint_connect(ep, server_addr);
    } while (ch == NULL);

    shared->full.wait(num_servers + 1);

    uint64_t begin = rdtsc64();
    for (int i = 0; i < NUM_MSG; ++i) {
        int * ri;
        __attribute__((unused)) size_t rs;
        int * si;

        si = (int*)channel_allocate(ch, sizeof(i));
        assert(si);
        *si = i;
        channel_send(ch, si, sizeof(i));

        rs = channel_receive_blocking(ch, (void**)&ri);
        assert(*ri == i);
        assert(rs == sizeof(i));
        channel_free(ch, ri);
    }
    uint64_t end = rdtsc64();

    shared->quit = true;

    shared->full.wait(num_servers + 1);

    shared->result = ((double) (end - begin)) / NUM_MSG;
    printf("[%d] %d -> %f\n", getpid(), id, shared->result);

    channel_close(ch);
    endpoint_destroy(ep);
}

int
main(int _argc, char *_argv[])
{
    argc = _argc;
    argv = _argv;

    if (argc < 3) {
        printf("%s: Runs many servers for a single client. Servers share a channel, created before fork().\n", argv[0]);
        printf("Usage: %s <client_core> <server_core>+\n", argv[0]);
        exit(-1);
    }

    num_servers = argc - 2;

    shared = (Shared *) mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(shared, 0, sizeof(Shared));
    assert(shared != MAP_FAILED);

    server_ep = endpoint_create();
    server_addr = endpoint_get_address(server_ep);

    pid_t p = fork();
    if (p == 0) {
        client(0);
        return 0;
    } else {
        shared->client = p;
    }

    do {
        server_ch = endpoint_accept(server_ep);
    } while(!server_ch);

    for (int i = 0; i < num_servers; i++) {
        p = fork();
        if (p == 0) {
            server(i);
            return 0;
        } else {
            shared->servers[i] = p;
        }
    }

    for (int i = 0; i < num_servers; ++i) {
        waitpid(shared->servers[i], NULL, 0);
    }
    waitpid(shared->client, NULL, 0);

    int total_served = 0;
    for (int i = 0; i < num_servers; i++) {
        total_served += shared->served[i];
    }
    printf("total served: %d\n", total_served);
    assert(total_served == NUM_MSG);

    endpoint_destroy(server_ep);
    munmap(shared, sizeof(Shared));
    return 0;
}

