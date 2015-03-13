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

const int MAX_PAIRS = 64;
const int NUM_MSG = 10000000;

int argc;
int num_pairs;
char ** argv;

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
    pid_t servers[ MAX_PAIRS ];
    pid_t clients[ MAX_PAIRS ];
    Address server_ep_addr[ MAX_PAIRS ];
    double result[ MAX_PAIRS ];
    Barrier full;
};
Shared * shared;

void server(int id) {
    int cpu;

    cpu = strtol(argv[id*2+1], NULL, 10);
    printf("[%d] server %d running on %d\n", getpid(), id, cpu);
    bind_to_core(cpu);

    Endpoint * ep = endpoint_create();
    shared->server_ep_addr[id] = endpoint_get_address(ep);

    shared->full.wait(num_pairs * 2);

    Channel * ch;
    do {
        ch = endpoint_accept(ep);
    } while (ch == NULL);

    wmb();

    shared->full.wait(num_pairs * 2);

    // uint64_t begin = rdtsc64();
    for (int i = 0; i < NUM_MSG; i++) {
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
    // uint64_t end = rdtsc64();

    shared->full.wait(num_pairs * 2);

    // printf("[%d] %d -> %f\n", getpid(), id, ((double) (end - begin)) / NUM_MSG);

    channel_close(ch);
    endpoint_destroy(ep);
}

void client(int id) {
    int cpu;

    cpu = strtol(argv[id*2+2], NULL, 10);
    printf("[%d] client %d running on %d\n", getpid(), id, cpu);
    bind_to_core(cpu);

    Endpoint * ep = endpoint_create();

    shared->full.wait(num_pairs * 2);

    Channel * ch;
    do {
        ch = endpoint_connect(ep, shared->server_ep_addr[id]);
    } while (ch == NULL);

    shared->full.wait(num_pairs * 2);

    uint64_t begin = rdtsc64();
    for (int i = 0; i < NUM_MSG; ++i) {
        int * ri;
        __attribute__((unused)) size_t rs;
        int * si;

        rs = channel_receive_blocking(ch, (void**)&ri);
        assert(*ri == i);
        assert(rs == sizeof(i));
        channel_free(ch, ri);

        si = (int*)channel_allocate(ch, sizeof(i));
        assert(si);
        *si = i;
        channel_send(ch, si, sizeof(i));
    }
    uint64_t end = rdtsc64();

    shared->full.wait(num_pairs * 2);

    shared->result[id] = ((double) (end - begin)) / NUM_MSG;
    printf("[%d] %d -> %f\n", getpid(), id, shared->result[id]);

    channel_close(ch);
    endpoint_destroy(ep);
}

int
main(int _argc, char *_argv[])
{
    argc = _argc;
    argv = _argv;

    if (argc < 3 || (argc % 2 != 1)) {
        printf("%s: Runs pairs of echo server/clients to test significance of locality.\n", argv[0]);
        printf("Usage: %s (<server_core> <client_core>)+\n", argv[0]);
        exit(-1);
    }

    num_pairs = (argc - 1) / 2;

    shared = (Shared *) mmap(NULL, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(shared != MAP_FAILED);

    for (int i = 0; i < num_pairs; i++) {
        shared->server_ep_addr[i] = (Address) ~0;

        pid_t p;

        p = fork();
        if (p == 0) {
            server(i);
            return 0;
        } else {
            shared->servers[i] = p;
        }

        p = fork();
        if (p == 0) {
            client(i);
            return 0;
        } else {
            shared->clients[i] = p;
        }
    }

    for (int i = 0; i < num_pairs; ++i) {
        waitpid(shared->servers[i], NULL, 0);
        waitpid(shared->clients[i], NULL, 0);
    }

    double average = 0;
    for (int i = 0; i < num_pairs; i++) {
        average += shared->result[i];
    }
    average /= num_pairs;

    printf("avg: %f\n", average);

    munmap(shared, sizeof(Shared));
    return 0;
}

