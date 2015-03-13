#include <cassert>
#include <common/tsc.h>
#include <cstdlib>
#include <fcntl.h>
#include <iomanip>
#include <iostream>
#include <messaging/channel.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <core/core.h>

#define TRIALS (100000)

int
main(int argc, char *argv[])
{
    assert(argc == 2);
    int procs = (int) strtol(argv[1], NULL, 10);

    pthread_barrier_t *start = (pthread_barrier_t *) mmap(NULL, sizeof(pthread_barrier_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(start != MAP_FAILED);

    pthread_barrierattr_t start_attr;
    pthread_barrierattr_init(&start_attr);
    pthread_barrierattr_setpshared(&start_attr, PTHREAD_PROCESS_SHARED);
    pthread_barrier_init(start, &start_attr, procs);
    pthread_barrierattr_destroy(&start_attr);

    for (int i = 0; i < procs; ++i) {
        pid_t pid = fork();
        assert(pid != -1);
        if (pid == 0) {
            bind_to_core(i+1);

            Endpoint *ep = endpoint_create();

            pthread_barrier_wait(start);

            uint64_t t = 0;
            for (int j = 0; j < TRIALS; ++j) {
                uint64_t tb = rdtsc64();
                __attribute__((unused)) Channel *chan = endpoint_connect(ep, 0xdeadbeef);
                uint64_t te = rdtsc64();
                channel_close(chan);
                t += te - tb;
            }

            endpoint_destroy(ep);

            std::cout << "[" << getpid() << "] client(" << i + 1 << ") " << std::setw(4) << t / TRIALS << " cycles" << std::endl;

            return 0;
        }
    }

    bind_to_core(0);

    Endpoint *ep;
    Channel *chan;

    ep = endpoint_create_fixed(0xdeadbeef);

    uint64_t t = 0;
    for (int i = 0; i < TRIALS * procs; ++i) {
        uint64_t tb = rdtsc64();
        while ((chan = endpoint_accept(ep)) == NULL)
            ;
        uint64_t te = rdtsc64();
        channel_close(chan);
        t += te - tb;
    }

    endpoint_destroy(ep);

    std::cout << "[" << getpid() << "] server(0) " << std::setw(4) << t / (TRIALS * procs) << " cycles" << std::endl;

    return 0;
}

