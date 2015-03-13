#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include "channel.h"
#include <core/core.h>

int * f1_cpu, f2_cpu;

#define ITERATIONS (500000)

uint64_t overhead = 0;
uint64_t begin = 0, end = 0;
pthread_barrier_t barrier;

int nsenders;
bool share_connection;

typedef struct {
    Channel * send;
    Channel * recv;
} Connection;

void connection_create(Connection * connection) {
    connection->send = channel_create(); assert(connection->send != NULL);
    connection->recv = channel_create(); assert(connection->recv != NULL);
}

void connection_destroy(Connection * connection) {
    channel_destroy(connection->send);
    channel_destroy(connection->recv);
}

Connection * c;

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

void *f1(void *arg)
{
    long long id = (long long)arg;
    Connection * conn;
    bind_to_core(f1_cpu[id]);

    if (share_connection) {
        conn = &c[0];
    } else {
        conn = &c[id];
    }
    
    if (id == 0 || !share_connection) {
        connection_create(conn);
    }

    pthread_barrier_wait(&barrier);

    for (int i = 0; i < ITERATIONS; i++) {
        int * ri;
        size_t rs;
        int * si;

        do {
            si = (int*)channel_allocate(conn->send, sizeof(i));
        } while (si == NULL);
        *si = i;
        channel_send(conn->send, si, sizeof(i));

        channel_receive(conn->recv, (void**)&ri, &rs);
        assert(share_connection || *ri == i);
        assert(rs == sizeof(i));
        channel_free(conn->recv, ri);
    }

    pthread_barrier_wait(&barrier);

    if (id == 0 || !share_connection) {
        connection_destroy(conn);
    }

    pthread_exit(NULL);
}

void *f2(void *arg)
{
    bind_to_core(f2_cpu);

    pthread_barrier_wait(&barrier);

    int pos = -1;
    int nchans = share_connection ? 1 : nsenders;

    begin = rdtsc64();
    for (int i = 0; i < ITERATIONS * nsenders; i++) {
        int * ri;
        size_t rs;
        int * si;
        Connection * conn;

        channel_select(&c[0].send, nchans, sizeof(Connection), true, (void**)&ri, &rs, &pos);
        conn = c + pos;

        si = (int*)channel_allocate(conn->recv, sizeof(i));
        assert(si);
        *si = *ri;
        channel_send(conn->recv, si, sizeof(i));

        channel_free(conn->send, ri);
    }
    end = rdtsc64();

    pthread_barrier_wait(&barrier);

    pthread_exit(NULL);
}

int main(int argc, char ** argv)
{
#ifdef CHANNEL_NO_ALLOCATOR
    printf("0.0\n");
    return 0;
#endif

    if (argc == 1) {
        printf("Usage: %s nsenders share_connection? receiver_cpu sender_cpus...\n", argv[0]);
    }

    assert(argc > 3);
    nsenders = atoi(argv[1]);
    share_connection = atoi(argv[2]);
    f2_cpu = atoi(argv[3]);

    assert(argc>=nsenders+4);

    f1_cpu = (int *)malloc(sizeof(*f1_cpu) * nsenders);
    c = (Connection *)malloc(sizeof(*c) * nsenders);

    for (int i = 0; i < nsenders; i++) {
        f1_cpu[i] = atoi(argv[4+i]);
    }

    for (int i = 0; i < 100; i++) {
        uint64_t begin = rdtsc64();
        uint64_t end = rdtsc64();
        overhead = overhead * i + end - begin;
        overhead /= i + 1;
    }

    pthread_t t1[nsenders], t2;

    pthread_barrier_init(&barrier, NULL, 1 + nsenders);

    for (int i = 0; i < nsenders; i++) {
        pthread_create(&t1[i], NULL, f1, (void*)(long long)i);
    }
    pthread_create(&t2, NULL, f2, NULL);

    for (int i = 0; i < nsenders; i++) {
        pthread_join(t1[i], NULL);
    }
    pthread_join(t2, NULL);

    pthread_barrier_destroy(&barrier);

    free(c);
    free(f1_cpu);

    /* printf("%llu\n", (unsigned long long) ITERATIONS); */
    /* printf("%llu\n", (unsigned long long) (end - begin - overhead)); */
    printf("%f\n", ((double) (end - begin - overhead))/(ITERATIONS*nsenders));

    return 0;
}

