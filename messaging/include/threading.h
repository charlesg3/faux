#ifndef THREADING_H
#define THREADING_H

#include <stdint.h>
#include <ucontext.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file threading.h
 * @brief A light-weight cooperative threading library.
 *
 * This library multiplexes a single execution context between several
 * independent threads of control. Through the dispatch library, it is
 * also integrated with the messaging system to give threads of
 * control for particular incoming messages.
 *
 * @todo Add error code return values to API. -nzb
 */

/**
 * @brief Function signature for a thread.
 *
 * Thread entry points must subscribe to this function signature.
 *
 * @param arg Void pointer to (optional) data for thread.
 */
typedef void (*ThreadTarget)(void *arg);

/**
 * @brief The size of a thread's stack.
 */
#define THREAD_STACK_SIZE (64*1024)

/**
 * @brief State for a given execution context.
 *
 * Contexts are owned by threads. The number of contexts is less than
 * or equal to the number of threads. New contexts are only created
 * when a thread sleeps and its state must be saved to resume later
 * execution. Users do not access this structure directly.
 */
typedef struct {
    ucontext_t context;
    uint8_t * top;
    uint8_t stack[THREAD_STACK_SIZE];
} Context;

/**
 * @brief State for a thread.
 *
 * This represents the state for a single thread. The user need not
 * concern themselves with the internals of this structure.
 */
typedef struct {
    int tid;
    Context * context;
    ThreadTarget target;
    void* arg;
} Thread;

/**
 * @brief Start the threading and transfer control to a thread.
 */
void threadingStart(ThreadTarget idle_target, void * idle_arg);

/**
 * @brief Terminate threading.
 *
 * Quits out of threading and immediately returns to who called
 * startThreading. */
void threadingStop();

/**
 * Is threading active?
 */
bool threadingIsRunning();

/**
 * @brief Create a new thread.
 *
 * Creates a new thread object (does not run it).  The threading
 * library will reclaim the memory when the thread returns 
 *
 * @param target The entry point for thread.
 * @param arg Parameter to pass to target.
 * @return A pointer to new thread.
 */
Thread * threadCreate(
    ThreadTarget target, 
    void *arg);

/**
 * @brief Switch to a different thread.
 *
 * Saves the state of the current thread, and switches to the given
 * thread (causing threadSwap on that thread to return rtnval)
 *
 * @param next_thread The thread to switch to.
 * @param rntval Value to return from threadSwap of other thread.
 */
void threadSwap(
    Thread * next_thread);

/**
 * @brief Switch to a different thread and terminate.
 *
 * Switches to a different thread and never returns.
 *
 * @param next_thread The thread to switch to.
 * @param rntval Value to return from threadSwap of other thread.
 */
void threadSwitch(
    Thread * next_thread);

/**
 * @brief Get the current thread.
 *
 * Gets the current thread.  Pointer is invalid once the thread
 * returns.
 *
 * @return Pointer to current thread.
 */
Thread* threadingGetCurrent();

/**
 * @brief Returns the number of active threads.
 *
 * @return The number of active threads.
 */
int threadingGetNumThreads();

/**
 * @brief Yield to another runnable thread or the main message loop.
 * @param in_runnable True if the current thread is runnable and should be
 *   re-scheduled, false if this thread will be re-activated via some
 *   other means. (Warning: passing false leaves this thread orphaned as
 *   far as the scheduler is concerned!)
 * @return Returns when the thread is re-scheduled. */
void threadYield(bool in_runnable);

/**
 * @brief Add a thread to the scheduler's list of runnable threads.
 *
 * Only call this if the thread is not currently runnable.
 *
 * @param in_thread The thread to mark runnable.
 */
void threadSchedule(Thread * in_thread);

/**
 * @brief Support for lists of threads in a circular queue.
 */
#define THREAD_LIST_SIZE (128)

typedef struct {
    size_t at;
    size_t size;
    Thread * threads[ THREAD_LIST_SIZE ];
} ThreadList;

static inline void threadListInitialize(ThreadList * list) {
    list->at = 0;
    list->size = 0;
}

static inline bool threadListPush(ThreadList * list, Thread * thread) {
    if (list->size == THREAD_LIST_SIZE) {
        return false;
    }

    size_t tail = list->at + list->size;
    tail %= THREAD_LIST_SIZE;
    list->threads[tail] = thread;

    ++list->size;
    return true;
}

static inline void threadListPop(ThreadList * list) {
    ++list->at;
    list->at %= THREAD_LIST_SIZE;
    --list->size;
}

static inline Thread * threadListFront(ThreadList * list) {
    return list->threads[list->at];
}

static inline size_t threadListSize(ThreadList * list) {
    return list->size;
}

/**
 * @brief Condition variables in the cooperative threading framework.
 */
typedef struct {
    ThreadList list;
} ConditionVariable;

void condInitialize(ConditionVariable *cv);
void condDestroy(ConditionVariable *cv);
void condWait(ConditionVariable *cv);
void condNotify(ConditionVariable *cv);
void condNotifyAll(ConditionVariable *cv);
bool condEmpty(ConditionVariable *cv);

#ifdef __cplusplus
}
#endif

#endif
