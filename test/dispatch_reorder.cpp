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

#define NUM_MSG (100000)

// really bizarre echo handler that receives another packet, concatenates them, and replies
// just to test reordering of messages in dispatch
void echo_handler(Channel * from, DispatchTransaction trans, void * message_a, size_t size_a) {
    void * message_b;
    size_t size_b;

    size_b = dispatchReceive(trans, &message_b);

    uint8_t * reply = (uint8_t *) dispatchAllocate(from, size_a + size_b);
    memcpy(reply, message_a, size_a);
    memcpy(reply + size_a, message_b, size_b);
    dispatchRespond(from, trans, reply, size_a + size_b);

    dispatchFree(trans, message_a);
    dispatchFree(trans, message_b);
}

void quit_handler(Channel * from, DispatchTransaction trans, void * message, size_t size) {
    dispatchFree(trans, message);
    dispatchStop();
}

int argc;
int num_clients;
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
    Address server_ep_addr;
    Barrier full, client;
};
Shared * shared;

void server() {
    int cpu;
    Endpoint *ep;

    cpu = strtol(argv[1], NULL, 10);
    printf("[%d] server running on %d\n", getpid(), cpu);
    bind_to_core(cpu);

    ep = endpoint_create();
    shared->server_ep_addr = endpoint_get_address(ep);
    wmb();

    shared->full.wait(num_clients + 1);

    dispatchListen(ep);
    dispatchRegister(0, echo_handler);
    dispatchRegister(1, quit_handler);

    uint64_t begin = rdtsc64();
    dispatchStart();
    uint64_t end = rdtsc64();

    shared->full.wait(num_clients + 1);
    shared->full.wait(num_clients + 1);

    printf("[%d] avg: %f\n", getpid(), ((double) (end - begin)) / (num_clients * NUM_MSG));

    dispatchUnregister(1);
    dispatchUnregister(0);
    dispatchIgnore(ep);
}

void client(int id) {
    int cpu;

    cpu = strtol(argv[2+id], NULL, 10);
    printf("[%d] client %d running on %d\n", getpid(), id, cpu);
    bind_to_core(cpu);

    shared->full.wait(num_clients + 1);

    Channel *server = dispatchOpen(shared->server_ep_addr);

    shared->client.wait(num_clients);

    uint64_t begin = rdtsc64();
    for (int i = 0; i < NUM_MSG; ++i) {
        int *msg;
        DispatchTransaction t;
        __attribute__((unused)) size_t size;

        msg = (int *) dispatchAllocate(server, sizeof(int));
        *msg = i;
        t = dispatchRequest(server, 0, msg, sizeof(int));

        msg = (int *) dispatchAllocate(server, sizeof(int));
        *msg = id;
        dispatchRespond(server, t, msg, sizeof(int));

        size = dispatchReceive(t, (void **) &msg);
        assert(size == sizeof(int) * 2 && msg[0] == i && msg[1] == id);
        dispatchFree(t, msg);
    }
    uint64_t end = rdtsc64();

    shared->client.wait(num_clients);

    // send quit message
    if (id == 0) {
        void *msg = dispatchAllocate(server, 1);
        dispatchRequest(server, 1, msg, 1);
    }

    shared->full.wait(num_clients + 1);
    printf("[%d] %f\n", getpid(), ((double) (end - begin)) / NUM_MSG);
    shared->full.wait(num_clients + 1);

    dispatchClose(server);
}

int
main(int _argc, char *_argv[])
{
    argc = _argc;
    argv = _argv;

    pid_t server_pid;

    num_clients = argc - 2;
    shared = (Shared *) mmap(NULL, sizeof(Address), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(shared != MAP_FAILED);
    shared->server_ep_addr = (Address) ~0;
    // shared->client_barrier = shared->full_barrier = 0;

    server_pid = fork();
    assert(server_pid != -1);
    if (server_pid) {
        server();
    } else {
        pid_t * clients = new pid_t[num_clients];

        for (int i = 0; i < num_clients; ++i) {
            pid_t client_pid = fork();
            if (client_pid != 0) {
                clients[i] = client_pid;
            } else {
                client(i);
                return 0;
            }
        }
        for (int i = 0; i < num_clients; ++i)
            waitpid(clients[i], NULL, 0);

        delete [] clients;
    }

    munmap(shared, sizeof(Address));
    return 0;
}

