#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <messaging/channel.h>
#include <common/tsc.h>
#include <core/core.h>

int f1_cpu, f2_cpu;

#define ITERATIONS (20000)

uint64_t begin = 0, end = 0;
pthread_barrier_t barrier;

Endpoint * s1, * s2;

static inline void echo_recv(Channel * ch, size_t s) {
    int * ri;
    __attribute__((unused)) size_t rs;
    rs = channel_receive_blocking(ch, (void**)&ri);
    assert(rs == s * sizeof(int));
    channel_free(ch, ri);
}

static inline void echo_send(Channel * ch, size_t s) {
    int * si;
    si = (int*)channel_allocate(ch, s * sizeof(int));
    assert(si);
    for (size_t i = 0; i < s; i ++) {
        si[i] = 0xffffffff;
    }
    channel_send(ch, si, s * sizeof(int));
}

void *f1(void *arg)
{
    bind_to_core(f1_cpu);

    Channel * ch;
    Address f2addr = endpoint_get_address(s2);
    ch = endpoint_connect(s1, f2addr);
    assert(f2addr == ch->remote_ep_addr);

    pthread_barrier_wait(&barrier);
    echo_send(ch, 1000);
    echo_recv(ch, 1000);

    for (int i = 0; i < ITERATIONS; i++) {
        echo_send(ch, 1);
        echo_recv(ch, 1);
    }

    pthread_barrier_wait(&barrier);

    channel_close(ch);

    pthread_exit(NULL);
}

void *f2(void *arg)
{
    bind_to_core(f2_cpu);

    Channel * ch;
    endpoint_get_address(s1);
    while (!(ch = endpoint_accept(s2)))
        ;
    pthread_barrier_wait(&barrier);

    begin = rdtsc64();
    echo_recv(ch, 1000);
    echo_send(ch, 1000);

    for (int i = 0; i < ITERATIONS; i++) {
        echo_recv(ch, 1);
        echo_send(ch, 1);
    }
    end = rdtsc64();

    pthread_barrier_wait(&barrier);

    channel_close(ch);

    pthread_exit(NULL);
}

int main(int argc, char ** argv)
{
    assert(argc==3);
    f1_cpu = atoi(argv[1]);
    f2_cpu = atoi(argv[2]);

    pthread_t t1, t2;

    s1 = endpoint_create();
    s2 = endpoint_create();

    pthread_barrier_init(&barrier, NULL, 2);

    pthread_create(&t1, NULL, f1, NULL);
    pthread_create(&t2, NULL, f2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pthread_barrier_destroy(&barrier);

    endpoint_destroy(s1);
    endpoint_destroy(s2);

    /* printf("%llu\n", (unsigned long long) ITERATIONS); */
    /* printf("%llu\n", (unsigned long long) (end - begin)); */
    printf("%f\n", ((double) (end - begin))/ITERATIONS);

    return 0;
}

