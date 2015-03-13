#pragma once

#include <sched.h>
#include "channel.h"
#include "threading.h"
#include "common/tsc.h"
#include <unistd.h>

extern "C" {

#include <sys/wait.h>
#include <stdio.h>
#include <utilities/debug.h>
#include <execinfo.h>

/* constants */

/* types */

typedef int DispatchHandlerType;

typedef struct {
    int id;
} DispatchTransaction;

typedef void (*DispatchHandler)(Channel *from, DispatchTransaction trans, void *message, size_t size);

/* API */

  /* mgmt */
void dispatchStart();
void dispatchStop();
bool dispatchIsRunning();
int dispatchCheckQueues();
void dispatchCheckNotifications();

void dispatchSetCloseCallback(void (*callback)(Channel *));

  /* handlers */
void dispatchListen(Endpoint *endpoint); /* monitor this endpoint for new channels and listen on all channels */
void dispatchIgnore(Endpoint *endpoint);
void dispatchListenChannel(Channel * channel); /* explicitly tell the dispatcher to monitor a channel created in some other fashion */
void dispatchIgnoreChannel(Channel * channel);
void dispatchRegister(DispatchHandlerType type, DispatchHandler handler);
void dispatchUnregister(DispatchHandlerType type);
void dispatchRegisterIdle(ThreadTarget tgt, void * arg);
void dispatchUnregisterIdle();

  /* messaging */
static void *dispatchAllocate(Channel *chan, size_t size);
static DispatchTransaction dispatchRequest(Channel *chan, DispatchHandlerType handler, void *data, size_t size);
static void dispatchRespond(Channel *chan, DispatchTransaction trans, void *data, size_t size);
size_t dispatchReceive(DispatchTransaction trans, void **data);
static void dispatchFree(DispatchTransaction trans, void *data);

  /* misc */
void dispatchSleep(int in_ms);

Channel * dispatchOpen(Address peer);
void dispatchClose(Channel *chan);
void dispatchRegisterResponseEndpoint(Endpoint *ep);

  /* implementation */

 /* Generate a unique transaction identifier */
static inline DispatchTransaction _dispatchGenerateTransaction() {
    DispatchTransaction trans;

    /* todo: better transaction id generation...practically, this
     * doens't conflict but there are no guarantees -nzb */
    static int salt = (int) getpid() << 16;
    uint32_t low_, high_;
    __asm__ __volatile__ ("rdtsc" : "=a" (low_), "=d" (high_));
    trans.id = salt ^ (int)(((uint64_t) high_ << 32) | (uint64_t) low_);

    return trans;
}

typedef union {
    struct {
        // -1 if this is a response
        // otherwise, the identifier of the handler to invoke
        DispatchHandlerType handler;

        // transaction id to wake up if this is a response
        // transaction id of requesting thread if this is a request
        DispatchTransaction transaction;

#if ENABLE_DBG_SHOW_QUEUE_DELAY
        // used for debugging purposes to see how much
        // time messages are sitting in the queue
        uint64_t send_time;
#endif
    } header;


    // The channel on which the message was received. Used only
    // receiver side _after_ the message has been processed.
    Channel * from;
} _DispatchHeader;

static inline void *dispatchAllocate(Channel *chan, size_t size) {
    uint8_t *buf = (uint8_t *) channel_allocate(chan, size + sizeof(_DispatchHeader));
    if (likely(buf != NULL)) {
        return (void *)(buf + sizeof(_DispatchHeader));
    } else {
        print_trace();
        assert(false);
        return NULL;
    }
}

static inline uint64_t __rdtscll()
{
    uint32_t low_, high_;

    __asm__ __volatile__ ("rdtsc" : "=a" (low_), "=d" (high_));
    return (((uint64_t) high_ << 32) | (uint64_t) low_);
}

static inline DispatchTransaction dispatchRequest(Channel *chan, DispatchHandlerType handler, void *data, size_t size) {
    // allocate new transaction
    DispatchTransaction dt = _dispatchGenerateTransaction();
    _DispatchHeader *hdr = (_DispatchHeader *)(((uint8_t *)data) - sizeof(_DispatchHeader));
    hdr->header.handler = handler;
    hdr->header.transaction = dt;
#if ENABLE_DBG_SHOW_QUEUE_DELAY
    hdr->header.send_time = __rdtscll();
#endif

    channel_send(chan, hdr, size + sizeof(_DispatchHeader));

    return dt;
}

static inline void dispatchRespond(Channel *chan, DispatchTransaction trans, void *data, size_t size) {
    _DispatchHeader *hdr = (_DispatchHeader *)(((uint8_t *)data) - sizeof(_DispatchHeader));
    hdr->header.handler = -1;
    hdr->header.transaction = trans;

    channel_send(chan, hdr, size + sizeof(_DispatchHeader));
}

static inline void dispatchNotify(Channel *chan, void *data, size_t size) {
    _DispatchHeader *hdr = (_DispatchHeader *)(((uint8_t *)data) - sizeof(_DispatchHeader));
    hdr->header.handler = -2;
    hdr->header.transaction.id = -1;

    // we don't notify exiting processes
    size_t pollsize = channel_poll(chan);
    if(pollsize == (size_t)-1) return;

    channel_send(chan, hdr, size + sizeof(_DispatchHeader));
    asm volatile ("mfence" : : : "memory");
}

static inline void dispatchFree(DispatchTransaction trans, void *data) {
    _DispatchHeader *hdr = (_DispatchHeader *)(((uint8_t *)data) - sizeof(_DispatchHeader));
    channel_free(hdr->from, hdr);
}

}
