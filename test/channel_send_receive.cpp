#include <assert.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <messaging/channel.h>

#include <common/tsc.h>
#include <common/util.h>
#include <core/core.h>

#define NUM_MSG (100000)

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
        Channel *chan;

        cpu = strtol(argv[1], NULL, 10);
        bind_to_core(cpu);

        ep = endpoint_create();
        *server_ep_addr = endpoint_get_address(ep);
        wmb();
        while ((chan = endpoint_accept(ep)) == NULL)
            ;
        uint64_t begin = rdtsc64();
        for (int i = 0; i < NUM_MSG; ++i) {
            int *msg;
            __attribute__((unused)) size_t size;

            size = channel_receive_blocking(chan, (void **) &msg);
            assert(*msg == i && size == sizeof(int));
            channel_free(chan, msg);

            while ((msg = (int *) channel_allocate(chan, sizeof(int))) == NULL)
                ;
            *msg = i;
            channel_send(chan, msg, sizeof(int));
        }
        uint64_t end = rdtsc64();
        channel_close(chan);
        endpoint_destroy(ep);
        printf("%8.3f\n", ((double) (end - begin)) / NUM_MSG);
    } else {
        Endpoint *ep;
        Channel *chan;

        cpu = strtol(argv[2], NULL, 10);
        bind_to_core(cpu);

        while (*server_ep_addr == (Address) ~0)
            cpu_relax();

        ep = endpoint_create();
        chan = endpoint_connect(ep, *server_ep_addr);
        for (int i = 0; i < NUM_MSG; ++i) {
            int *msg;
            __attribute__((unused)) size_t size;

            while ((msg = (int *) channel_allocate(chan, sizeof(int))) == NULL)
                ;
            *msg = i;
            channel_send(chan, msg, sizeof(int));

            size = channel_receive_blocking(chan, (void **) &msg);
            assert(*msg == i && size == sizeof(int));
            channel_free(chan, msg);
        }
        channel_close(chan);
        endpoint_destroy(ep);
    }

    munmap(server_ep_addr, sizeof(Address));
    return 0;
}

