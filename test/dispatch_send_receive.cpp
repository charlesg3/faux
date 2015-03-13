#include <assert.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <messaging/dispatch.h>
#include <common/tsc.h>
#include <common/util.h>
#include <core/core.h>

#define NUM_MSG (100000)

void
echo_handler(Channel * from, DispatchTransaction trans, void * message, size_t size)
{
    void * reply = dispatchAllocate(from, size);
    memcpy(reply, message, size);
    dispatchRespond(from, trans, reply, size);
    dispatchFree(trans, message);
}

void
quit_handler(Channel * from, DispatchTransaction trans, void * message, size_t size)
{
    dispatchFree(trans, message);
    dispatchStop();
}

int
main(int argc, char *argv[])
{
    Address *server_ep_addr;
    pid_t pid;
    int cpu;

    server_ep_addr = (Address *) mmap(NULL, sizeof(Address), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(server_ep_addr != MAP_FAILED);
    *server_ep_addr = (Address) ~0;

    pid = fork();
    assert(pid != -1);
    if (pid) {
        Endpoint *ep;

        cpu = strtol(argv[1], NULL, 10);
        bind_to_core(cpu);

        ep = endpoint_create();
        *server_ep_addr = endpoint_get_address(ep);
        wmb();

        dispatchListen(ep);
        dispatchRegister(0, echo_handler);
        dispatchRegister(1, quit_handler);

        uint64_t begin = rdtsc64();
        dispatchStart();
        uint64_t end = rdtsc64();

        dispatchUnregister(1);
        dispatchUnregister(0);
        dispatchIgnore(ep);

        printf("%f\n", ((double) (end - begin)) / NUM_MSG);
    } else {
        cpu = strtol(argv[2], NULL, 10);
        bind_to_core(cpu);

        while (*server_ep_addr == (Address) ~0)
            cpu_relax();

        Channel *server = dispatchOpen(*server_ep_addr);

        for (int i = 0; i < NUM_MSG; ++i) {
            int *msg;
            DispatchTransaction t;
            __attribute__((unused)) size_t size;

            msg = (int *) dispatchAllocate(server, sizeof(int));
            *msg = i;
            t = dispatchRequest(server, 0, msg, sizeof(int));

            size = dispatchReceive(t, (void **) &msg);
            assert(*msg == i && size == sizeof(int));
            dispatchFree(t, msg);
        }

        void *msg = dispatchAllocate(server, 1);
        dispatchRequest(server, 1, msg, 1);

        dispatchClose(server);
    }

    munmap(server_ep_addr, sizeof(Address));
    return 0;
}

