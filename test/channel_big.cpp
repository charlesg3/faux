#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <messaging/channel.h>
#include <core/core.h>

int f1_cpu, f2_cpu;

#define ITERATIONS (100000)
#define SIZE (1024)

uint64_t overhead = 0;
uint64_t begin = 0, end = 0;
pthread_barrier_t barrier;

Channel * c1, * c2;

static inline uint64_t rdtsc64(void)
{
    uint32_t lo, hi;

    __asm__ __volatile__
    (
            "mfence\n"
            "rdtsc"
            : "=a" (lo), "=d" (hi)
            :
            : "rbx", "rcx"
    );
    return (uint64_t) (((uint64_t) hi) << 32 | (uint64_t) lo);
}

uint8_t message[SIZE];

void *f1(void *arg)
{
    bind_to_core(f1_cpu);

    c1 = channel_create(); assert(c1);
    c2 = channel_create(); assert(c2);

    pthread_barrier_wait(&barrier);

    for (int i = 0; i < ITERATIONS; i++) {
        uint8_t * ri;
        size_t rs;
        uint8_t * si;

#ifndef NDEBUG
        message[i % SIZE] = i;
#endif

        si = (uint8_t*)channel_allocate(c1, SIZE);
        assert(si);
        __builtin_memcpy(si, message, SIZE);
        channel_send(c1, si, SIZE);

        channel_receive(c2, (void**)&ri, &rs);
        assert(rs == SIZE);
        assert(ri[i % SIZE] == (uint8_t)i);
        channel_free(c2, ri);
    }

    pthread_barrier_wait(&barrier);

    channel_destroy(c1);
    channel_destroy(c2);

    pthread_exit(NULL);
}

void *f2(void *arg)
{
    bind_to_core(f2_cpu);

    pthread_barrier_wait(&barrier);

    begin = rdtsc64();
    for (int i = 0; i < ITERATIONS; i++) {
        uint8_t * ri;
        size_t rs;
        uint8_t * si;

        channel_receive(c1, (void**)&ri, &rs);
        assert(rs == SIZE);
        assert(ri[i % SIZE] == (uint8_t)i);

        si = (uint8_t*)channel_allocate(c2, rs);
        assert(si);
        __builtin_memcpy(si, ri, rs);
        channel_send(c2, si, rs);
        channel_free(c1, ri);
    }
    end = rdtsc64();

    pthread_barrier_wait(&barrier);

    pthread_exit(NULL);
}

int main(int argc, char ** argv)
{
    assert(argc==3);
    f1_cpu = atoi(argv[1]);
    f2_cpu = atoi(argv[2]);

    for (int i = 0; i < 100; i++) {
        uint64_t begin = rdtsc64();
        uint64_t end = rdtsc64();
        overhead = overhead * i + end - begin;
        overhead /= i + 1;
    }

    pthread_t t1, t2;

    pthread_barrier_init(&barrier, NULL, 2);

    pthread_create(&t1, NULL, f1, NULL);
    pthread_create(&t2, NULL, f2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pthread_barrier_destroy(&barrier);

    /* printf("%llu\n", (unsigned long long) ITERATIONS); */
    /* printf("%llu\n", (unsigned long long) (end - begin - overhead)); */
    printf("%f\n", ((double) (end - begin - overhead))/ITERATIONS);

    return 0;
}

