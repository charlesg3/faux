#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <messaging/threading.h>
#include <common/pool.hpp>
#include <common/common.h>
#include <common/likely.h>

    /* macros  */
#ifdef PAGE_SIZE
#error PAGE_SIZE already defined
#else
#define PAGE_SIZE 4096
#endif

#define mb()    __asm__ __volatile__ ("mfence":::"memory")
#define rmb()   __asm__ __volatile__ ("lfence":::"memory")
#define wmb()	__asm__ __volatile__ ("sfence" ::: "memory")

    /* state */
static ThreadTarget main_target;
static void * main_arg;

static int tid;
static Thread * current_thread;
static ucontext_t exit_context;
static ThreadList ready_threads;

static const int THREAD_POOL_SIZE = 128;
static Pool<Thread, THREAD_POOL_SIZE> thread_pool;
static Pool<Context, THREAD_POOL_SIZE> context_pool;

  /* Helpers */

static Thread * _threadGetRunnable() {
    if (threadListSize(&ready_threads) > 0) {
        Thread * rtn = threadListFront(&ready_threads);
        threadListPop(&ready_threads);
        return rtn;
    } else {
        return threadCreate(main_target, main_arg);
    }
}

static void _threadDestroy(Thread* t) {
    if (t->context) {
        context_pool.free(t->context);
    }
    thread_pool.free(t);
}

// The goal of this main loop design is to avoid capturing and
// swapping contexts as much as possible. This is done by keeping
// threads and contexts separate. New contexts are only made and
// swapped when a thread yields from threadYield.
// 
// We guarantee that there is only one active thread.
static void _threadMain(void * vp_next_thread) {
    assert(current_thread && current_thread->context);

    Thread * next_thread = (Thread *) vp_next_thread;

    while (true) {
        if (next_thread == NULL) {
            next_thread = _threadGetRunnable();
        }

        if (next_thread->context != NULL) {
            threadSwitch(next_thread);
        } else {
            // The thread we are swapping to has no context
            // created. Avoid creating a context for it by re-using
            // the current one and simply calling the target.
            next_thread->context = current_thread->context;
            current_thread->context = NULL;
            _threadDestroy(current_thread);
            current_thread = next_thread;

            current_thread->target(current_thread->arg);
        }

        next_thread = NULL;
    }
}

static void _threadMakeContext(Thread * t) {
    Context * context;

    if (t->context == NULL) {
        context = context_pool.allocate();
    } else {
        context = t->context;
    }

    assert(context != NULL);
    assert(context_pool.available() >= thread_pool.available());

    getcontext(&context->context);
    context->context.uc_link = NULL; // threads will never return
    context->context.uc_stack.ss_sp = context->stack;
    context->context.uc_stack.ss_size = THREAD_STACK_SIZE;
    makecontext(&context->context, (void (*)()) t->target, 1, t->arg);

    t->context = context;
}

  /* API implementation */

void threadingStart(ThreadTarget main, void* arg) {
    main_target = main;
    main_arg = arg;

    Thread * next_thread = threadCreate(_threadMain, NULL);
    current_thread = (Thread *) 0xdeadbeef; // prevent loop when swapping back to exit_context
    getcontext(&exit_context);

    if (current_thread != NULL) {
        current_thread = NULL;
        threadSwitch(next_thread);
    }
}

void threadingStop() {
    if (threadingGetNumThreads() > 1) {
        error("There are still %d active threads.", threadingGetNumThreads());
    }
    _threadDestroy(current_thread);
    current_thread = NULL;
    setcontext(&exit_context);
}

void threadSwap(Thread * next_thread) {
    assert(current_thread != NULL);
    assert(current_thread->context != NULL);
    assert(next_thread != NULL);

    if (next_thread->context == NULL) {
        _threadMakeContext(next_thread);
    }

    Thread * saved = current_thread;
    current_thread = next_thread;
    swapcontext(&saved->context->context, &next_thread->context->context);
}

void threadSwitch(Thread * next_thread) {
    // assert(current_thread != NULL);
    assert(next_thread != NULL);

    if (next_thread == current_thread) {
        return;
    }

    if (current_thread != NULL) {
        _threadDestroy(current_thread);
    }

    if (next_thread->context == NULL) {
        _threadMakeContext(next_thread);
    }

    current_thread = next_thread;
    setcontext(&next_thread->context->context);
    for (;;);                   // no return
}

Thread* threadCreate(ThreadTarget target, void* arg) {
    Thread * thread = thread_pool.allocate();
    if (unlikely(thread == NULL)) {
        error("Ran out of threads in the pool! Increase the number of threads allowed or decrease concurrency.");
    }

    thread->tid = ++tid;
    thread->context = NULL;
    thread->target = target;
    thread->arg = arg;

    return thread;
}

Thread* threadingGetCurrent() {
    return current_thread;
}

int threadingGetNumThreads() {
    return threadListSize(&ready_threads) + 1;
}

bool threadingIsRunning() {
    return threadingGetCurrent() != NULL;
}

void threadSchedule(Thread* t) {
    if (!threadListPush(&ready_threads, t)) {
        error("Too many threads ready! (%lu threads ready)", (unsigned long)  threadListSize(&ready_threads));
    }
}

void threadYield(bool runnable) {
    assert(threadingIsRunning());

    Thread * next_thread;

    if (runnable) {
        if (threadListSize(&ready_threads) > 0) {
            threadSchedule(threadingGetCurrent());
        } else {
            return;
        }
    }

    if (threadListSize(&ready_threads) > 0) {
        next_thread = _threadGetRunnable();
        if (next_thread->context == NULL) {
            next_thread = threadCreate(_threadMain, (void *) next_thread);
        }
    } else {
        next_thread = threadCreate(_threadMain, NULL);
    }
    threadSwap(next_thread);
}

void condInitialize(ConditionVariable *cv) {
    threadListInitialize(&cv->list);
}

void condDestroy(ConditionVariable * cv) {
}

void condWait(ConditionVariable *cv) {
    threadListPush(&cv->list, threadingGetCurrent());
    threadYield(false);
}

void condNotify(ConditionVariable *cv) {
    if (condEmpty(cv)) {
        return;
    }

    threadSchedule(threadListFront(&cv->list));
    threadListPop(&cv->list);
}

void condNotifyAll(ConditionVariable *cv) {
    while (!condEmpty(cv)) {
        condNotify(cv);
    }
}

bool condEmpty(ConditionVariable *cv) {
    return threadListSize(&cv->list) == 0;
}
