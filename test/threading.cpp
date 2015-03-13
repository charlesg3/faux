#include "threading.h"
#include "common.h"
#include "tsc.h"
#include <stdio.h>
#include <assert.h>

#define MAX (100000)
/* #define MAX (20) */

/* #define VERBOSE */
#ifdef VERBOSE
#define D() do { puts(__FUNCTION__); } while (0)
#else
#define D()
#endif

int i;

void idle(void * vp) {
    threadingStop();
}

void thread(void * vp) {
    D();
    if (++i < MAX) {
        threadSchedule(threadCreate(thread, NULL));
    }
}

void thread_yield(void * vp) {
    threadYield(true);
    D();
    if (++i < MAX) {
        threadSchedule(threadCreate(thread_yield, NULL));
    }
}

void thread_yield_long(void * vp) {
    while (++i < MAX) {
        D();
        threadYield(true);
    }
}

int main() {
    uint64_t end, begin;

    /* loop */
    i = 0;
    begin = rdtsc64();
    for ( ; i < MAX; ++i) {
        __asm__ __volatile__ ("nop");
    }
    end = rdtsc64();
    printf("loop - %f\n", ((double)(end-begin))/i);

    /* without yielding */
    i = 0;
    threadSchedule(threadCreate(thread, NULL));
    begin = rdtsc64();
    threadingStart(idle, NULL);
    end = rdtsc64();

    printf("minimal - %f\n", ((double)(end-begin))/i);

    /* two threads without yielding */
    i = 0;
    threadSchedule(threadCreate(thread, NULL));
    threadSchedule(threadCreate(thread, NULL));
    begin = rdtsc64();
    threadingStart(idle, NULL);
    end = rdtsc64();

    printf("double - %f\n", ((double)(end-begin))/i);

    /* with yielding */
    i = 0;
    threadSchedule(threadCreate(thread_yield, NULL));
    begin = rdtsc64();
    threadingStart(idle, NULL);
    end = rdtsc64();

    printf("yield - %f\n", ((double)(end-begin))/i);

    /* with yielding */
    i = 0;
    threadSchedule(threadCreate(thread_yield, NULL));
    threadSchedule(threadCreate(thread_yield, NULL));
    begin = rdtsc64();
    threadingStart(idle, NULL);
    end = rdtsc64();

    printf("yield x2 - %f\n", ((double)(end-begin))/i);

    /* with yielding, long-lived */
    i = 0;
    threadSchedule(threadCreate(thread_yield_long, NULL));
    threadSchedule(threadCreate(thread_yield_long, NULL));
    begin = rdtsc64();
    threadingStart(idle, NULL);
    end = rdtsc64();

    printf("yield [long] - %f\n", ((double)(end-begin))/i);

    /* long & short */
    i = 0;
    threadSchedule(threadCreate(thread_yield_long, NULL));
    threadSchedule(threadCreate(thread, NULL));
    begin = rdtsc64();
    threadingStart(idle, NULL);
    end = rdtsc64();

    printf("long & short - %f\n", ((double)(end-begin))/i);

    /* yield & short */
    i = 0;
    threadSchedule(threadCreate(thread_yield, NULL));
    threadSchedule(threadCreate(thread, NULL));
    begin = rdtsc64();
    threadingStart(idle, NULL);
    end = rdtsc64();

    printf("yield & short - %f\n", ((double)(end-begin))/i);

    /* long & yield */
    i = 0;
    threadSchedule(threadCreate(thread_yield, NULL));
    threadSchedule(threadCreate(thread_yield_long, NULL));
    begin = rdtsc64();
    threadingStart(idle, NULL);
    end = rdtsc64();

    printf("long & yield - %f\n", ((double)(end-begin))/i);

    return 0;
}
