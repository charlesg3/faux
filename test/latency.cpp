#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <core/core.h>

#define ITERATIONS (1000000)

volatile uint64_t * val = NULL;
/* volatile uint64_t * val2 = NULL; */
uint64_t overhead = 0;
uint64_t begin = 0, end = 0;
pthread_barrier_t barrier;

int f1_cpu, f2_cpu;

static inline uint64_t rdtsc64(void)
{
    uint32_t lo, hi;

    __asm__ __volatile__
    (
            "mfence\n"
            "rdtsc"
            : "=a" (lo), "=d" (hi)
    );
    return (uint64_t) (((uint64_t) hi) << 32 | (uint64_t) lo);
}

void *f1(void *arg)
{
    bind_to_core(f1_cpu);

    val = (uint64_t*)malloc(sizeof(uint64_t) * 8);
    val[0] = 0;
    /* val2 = malloc(sizeof(uint64_t) * 8); */
    /* val2[0] = 0; */

    pthread_barrier_wait(&barrier);

    while (val[0] < ITERATIONS) {
        if ((val[0] & 0x1) == 0) {
            val[0] = val[0]+1;
        }
    }

    pthread_barrier_wait(&barrier);

    free((void *)val);
    /* free((void *)val2); */

    pthread_exit(NULL);
}

void *f2(void *arg)
{
    bind_to_core(f2_cpu);

    pthread_barrier_wait(&barrier);

    begin = rdtsc64();
    while (val[0] < ITERATIONS) {
        if ((val[0] & 0x1) == 1) {
            val[0] = val[0]+1;
        }
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
        uint64_t begin_ = rdtsc64();
        uint64_t end_ = rdtsc64();
        overhead = overhead * i + end_ - begin_;
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
    printf("%f\n", ((double) 2 * (end - begin - overhead))/ITERATIONS);

    return 0;
}

