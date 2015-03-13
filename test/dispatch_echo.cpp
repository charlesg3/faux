#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include <config/config.h>
#include <messaging/dispatch.h>
//#include "tsc.h"
//#include "common.h"

int f1_cpu, f2_cpu;

#define ITERATIONS (10000000)

Endpoint *s1;
Endpoint *s2;

volatile static struct sharedmem {
    volatile bool done;
} *shared;

static inline unsigned long long ts_to_ms(unsigned long long cycles)
{
    return cycles / (2000000);
}

static inline unsigned long long ts_to_us(unsigned long long cycles)
{
    return cycles / (2000);
}

void bind_to_core(int core)
{
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(core, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);

    while(sched_getcpu() != core)
        usleep(0);
}

void *f3(void *arg)
{
    enable_pcid();
    bind_to_core(f2_cpu);
    while(!shared->done);
    struct rusage usage_end;
    getrusage(RUSAGE_SELF, &usage_end);
    printf("p_ru: %llu.%06llu\n", (uint64_t)usage_end.ru_utime.tv_sec, (uint64_t)usage_end.ru_utime.tv_usec);
    exit(0);
}

void *f1(void *arg)
{
    enable_pcid();
    struct sched_param params;
    params.sched_priority = 1;
    int ret = sched_setscheduler(0, SCHED_RR, &params);
    if(ret != 0)
        fprintf(stderr, "error setting scheduler.\n");
    /* client */
    uint64_t begin = 0, end = 0;
    bind_to_core(f1_cpu);

    usleep(100000);

    Channel * server = dispatchOpen(endpoint_get_address(s2));

    begin = rdtsc64();
    for (int i = 0; i < ITERATIONS; i++) {
//        sched_yield();

        DispatchTransaction trans;
        int * message = (int *)dispatchAllocate(server, sizeof(i));
        *message = i;
        trans = dispatchRequest(server, 0, message, sizeof(i));

        hare_wake(f2_cpu);

        void * reply;
        size_t size = dispatchReceive(trans, &reply);
        assert(size == sizeof(i));
        assert(*((int*)reply) == i);
        dispatchFree(trans, reply);
    }
    end = rdtsc64();
    struct rusage usage_end;
    getrusage(RUSAGE_SELF, &usage_end);

    shared->done = true;

    void * quit_message = dispatchAllocate(server, 1);
    dispatchRequest(server, 1, quit_message, 1);

    dispatchClose(server);

    printf("client_ru: %llu.%06llu\n", (uint64_t)usage_end.ru_utime.tv_sec, (uint64_t)usage_end.ru_utime.tv_usec);
    printf("us per rpc: %.2f\n", (ts_to_us((double) (end - begin))/(ITERATIONS * 1.0)));

    exit(0);
}

void echo_handler(Channel * from, DispatchTransaction trans, void * message, size_t size) {
    void * reply = dispatchAllocate(from, size);
    memcpy(reply, message, size);
    dispatchRespond(from, trans, reply, size);
    dispatchFree(trans, message);
}

void quit_handler(Channel * from, DispatchTransaction trans, void * message, size_t size) {
    struct rusage usage_end;
    getrusage(RUSAGE_SELF, &usage_end);
    printf("server_ru: %llu.%06llu cs: %llu\n", (uint64_t)usage_end.ru_utime.tv_sec, (uint64_t)usage_end.ru_utime.tv_usec, (uint64_t)usage_end.ru_nvcsw + usage_end.ru_nivcsw);
    dispatchFree(trans, message);
    dispatchStop();
}

void idle(void *arg)
{
//    sched_yield();
    /*
    int rc;
    pid_t child = waitpid(-1, &rc, WNOHANG);


    if(child == -1) return;
    if(child == 0) return;
    */
}

void *f2(void *arg)
{
    enable_pcid();
    struct sched_param params;
    params.sched_priority = 1;
    int ret = sched_setscheduler(0, SCHED_RR, &params);
    if(ret != 0)
        fprintf(stderr, "error setting scheduler.\n");
    bind_to_core(f2_cpu);
/*
    while(!shared->done)
        sched_yield();

    exit(0);
*/
    /* server */

    dispatchListen(s2);
    dispatchRegister(0, echo_handler);
    dispatchRegister(1, quit_handler);
//    dispatchRegisterIdle(idle, NULL);

    dispatchStart();

    dispatchUnregister(1);
    dispatchUnregister(0);
    dispatchIgnore(s2);

    exit(0);
}


int main(int argc, char ** argv)
{
    assert(argc==3);
    f1_cpu = atoi(argv[1]);
    f2_cpu = atoi(argv[2]);

    bind_to_core(f1_cpu);

    struct sched_param params;
    params.sched_priority = 1;
    int ret = sched_setscheduler(0, SCHED_RR, &params);
    if(ret != 0)
        fprintf(stderr, "error setting scheduler.\n");

    shared = (struct sharedmem *) mmap(0, sizeof(struct sharedmem), PROT_READ|PROT_WRITE, 
                                       MAP_SHARED|MAP_ANONYMOUS, 0, 0);

    shared->done = false;

    pid_t children[3];

    s1 = endpoint_create();
    s2 = endpoint_create();

    children[0] = fork();
    if(children[0] == 0)
        f1(NULL);

    children[1] = fork();
    if(children[1] == 0)
        f2(NULL);

//    children[2] = fork();
//    if(children[2] == 0)
//        f3(NULL);

    //for(int i = 0; i < 2; i++)
    for(int i = 0; i < 1; i++)
        waitpid(children[i], NULL, 0);

    shared->done = true;
    waitpid(children[1], NULL, 0);
//    waitpid(children[2], NULL, 0);

    endpoint_destroy(s1);
    endpoint_destroy(s2);


    return 0;
}
