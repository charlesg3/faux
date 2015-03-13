#pragma once

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <emmintrin.h>
#include <list>
#include <tr1/unordered_map>

#include <common/param.h>
#include <common/util.h>

/* #define CHANNEL_DEBUG */
#ifdef CHANNEL_DEBUG
#include <sys/syscall.h>
#include <unistd.h>
#endif  /* DEBUG */

#include <config/config.h>
#include "channel_conf.h"

// interface for using messaging. Basically the
// c portion of messaging for parts of the system
// that need to know about the types

#include "channel_interface.h"

typedef struct {
    Address local_ep_addr;

    uintptr_t local_ep_buf;
    Channel *chanp[ENDPOINT_PUBLIC_CHANNELS];
    Channel chan[ENDPOINT_PUBLIC_CHANNELS];
    Channel side_chan;

    int ep_size;

    uintptr_t local_chan_buf;
    std::list<uintptr_t> local_chan_pool;

    std::tr1::unordered_map<Address, uintptr_t> remote_ep_cache;
} Endpoint;

// Macros

#if defined(CHANNEL_PULL_INTO_CACHE_WRITE) && defined(CHANNEL_PULL_INTO_CACHE_CMPXCHG)
#error "Choose a single method to pull into cache, not both."
#endif

#ifdef CHANNEL_PULL_INTO_CACHE_WRITE
#define CHANNEL_MSG_READY(msg, val) (msg->dummy = 1, msg->ready == val)
#elif defined(CHANNEL_PULL_INTO_CACHE_CMPXCHG)
#define CHANNEL_MSG_READY(msg, val) (cmpxchg(&msg->ready, val, val))
#else
#define CHANNEL_MSG_READY(msg, val) (msg->ready == val)
#endif
#define CHANNEL_MSG_FROM_PAYLOAD(p)  (&((Message*)p)[-1])
#define CHANNEL_SHARED(chan, off) ((Message*)(&chan->shared[off]))
#define CHANNEL_SCRATCH(chan, off) ((Message*)(&chan->scratch[off]))
#define CHANNEL_MSG_FORTH(message, offset) ((Message*)(((uint8_t*)message)+offset))

// API

// endpoints and connections
Endpoint *endpoint_create_fixed(Address local_ep_address);
Endpoint *endpoint_create();
Endpoint *client_endpoint_create(); // this allocates an endpoint for a client with a much much smaller memory pool
void endpoint_destroy(Endpoint *ep);
Address endpoint_get_address(const Endpoint *ep);
Channel *endpoint_accept(Endpoint *ep);
Channel *endpoint_connect(Endpoint *ep, Address remote_ep_addr);
Address channel_get_local_address(const Channel *chan);
Address channel_get_remote_address(const Channel *chan);
void channel_close(Channel *chan);

// messaging
// sender
static inline void *channel_allocate(Channel *chan, size_t size);
static inline void channel_send(Channel *chan, void *data, size_t size);
// receiver
static inline size_t channel_poll(Channel *chan);
static inline size_t channel_receive(Channel *chan, void **data);
static inline size_t channel_receive_blocking(Channel *chan, void **data);
static inline size_t channel_select(Channel **chans, int nchans, uintptr_t stride, bool blocking, void **data, int *pos);
static inline void channel_free(Channel *chan, void *data);


// Implementation

    static inline size_t
_channel_message_size(size_t size)
{
    size_t msg_size = size + sizeof(Message);

    // FIXME: this is very ugly... we should find a way to use the whole
    // buffer for a single message
    if (msg_size > SHARED_SIZE - CACHELINE_SIZE)
        return 0;

    if ((msg_size & CACHELINE_MASK) != 0)
        msg_size = (msg_size & ~CACHELINE_MASK) + CACHELINE_SIZE;
    return msg_size;
}

    static inline void
_channel_gc(HalfChannel *chan)
{
    size_t head = chan->head;
    size_t tail = chan->tail;
    size_t freeing = tail+1;
    size_t freed = tail + 1 - head;
    if (head > tail)
        freed = SHARED_SIZE - freed;
    if (freed > SHARED_SIZE)
        freed = 0;

    // update tail by skipping past all freed chunks
    while (true) {
        if (freeing == head || freed == SHARED_SIZE) {
            // channel empty! don't run past head or loop through a free
            // channel indefinitely
            return;
        }

        if (unlikely(freeing >= SHARED_SIZE)) {
            freeing = 0;
        }

        Message * message = CHANNEL_SHARED(chan, freeing);
        if (CHANNEL_MSG_READY(message, true)) {
            // not freed
            return;
        }

        uint16_t size = message->size;
        uint16_t alloced = message->alloced;
        size_t new_tail;

        if (likely(size != (uint16_t) ~0)) {
            new_tail = freeing + alloced - 1;
        } else {
            // sentinel for wrapped allocation
            new_tail = SHARED_SIZE - 1;
        }

        if (!cmpxchg(&chan->tail, tail, new_tail)) {
            return;
        }
        freed += alloced;
        tail = new_tail;
        freeing = tail + 1;
    }
}

// A memcpy that copies cache all cache lines between src and dest. It will
// copy the headers of the message (first sizeof(Message) bytes) last
    static inline void
_channel_memcpy_cache_aligned(void * vp_dest, void * vp_src, size_t size)
{
#ifdef CHANNEL_USE_SSE
    // fast copy using SSE, but its actually super slow
    __m128i * src = (__m128i*) vp_src;
    __m128i * end = (__m128i*)((uint8_t*)vp_src + size);
    __m128i * dest = (__m128i*) vp_dest;

    // copy payload of message, then the line containing header
    __m128i * src_itr = src + 4;
    __m128i * dest_itr = dest + 4;
    for ( ; src_itr < end; src_itr += 4, dest_itr += 4) {
        __m128i temp3 = _mm_load_si128(src_itr + 3);
        __m128i temp2 = _mm_load_si128(src_itr + 2);
        __m128i temp1 = _mm_load_si128(src_itr + 1);
        __m128i temp0 = _mm_load_si128(src_itr + 0);
        _mm_store_si128(dest_itr + 3, temp3);
        _mm_store_si128(dest_itr + 2, temp2);
        _mm_store_si128(dest_itr + 1, temp1);
        _mm_store_si128(dest_itr + 0, temp0);
    }
    __m128i temp3 = _mm_load_si128(src + 3);
    __m128i temp2 = _mm_load_si128(src + 2);
    __m128i temp1 = _mm_load_si128(src + 1);
    __m128i temp0 = _mm_load_si128(src + 0);
    _mm_store_si128(dest + 3, temp3);
    _mm_store_si128(dest + 2, temp2);
    _mm_store_si128(dest + 1, temp1);
    _mm_store_si128(dest + 0, temp0);
    // streaming seems to go to memory or something and performs very badly -nzb
    // _mm_stream_si128(dest+0, temp0);
#else
    uint8_t * src = (uint8_t*) vp_src;
    uint8_t * dest = (uint8_t*) vp_dest;
    __builtin_memcpy(dest + sizeof(Message), src + sizeof(Message), size - sizeof(Message));
    __builtin_memcpy(dest, src, sizeof(Message));
#endif // SSE
}

    static inline void *
channel_allocate(Channel *chan, size_t size)
{
    assert(chan != NULL);
    assert(chan->out != NULL);

    HalfChannel *out = chan->out;
    size_t head = out->head;
    size_t new_head;
    // don't need to worry about races on tail, any update will only mean there
    // is more space available -nzb
    size_t tail = out->tail;

    size_t alloced_start;
    size_t alloced_size;
    bool wrapped = false;

    alloced_size = _channel_message_size(size);
    if (alloced_size == 0)
        return NULL;

#ifndef CHANNEL_NO_ALLOCATOR
    if (tail == SHARED_SIZE) {
        // XXX: this if should be triggered iff the buffer has been perfectly
        // filled, which means, tail is SHARED_SIZE and head has been brought
        // back to 0 -siro
        _channel_gc(out);
        tail = out->tail;
    }
    if (((head < tail) && (alloced_size <= tail + 1 - head))
            || ((head > tail) && (alloced_size <= SHARED_SIZE - head))) {
        alloced_start = head;
    } else if ((head > tail) && (alloced_size <= tail + 1)) {
        wrapped = true;
        alloced_start = 0;
    } else {
        // XXX: dead-lock!
        // Suppose the sender/receiver agrees to exchange N messages before
        // the receiver start freeing them and the shared channel is not big
        // enough, the sender/receiver may remain stuck in a dead-lock.
        // Maybe a "special" message that tells the receiver to free messages
        // can solve the problem -siro
        _channel_gc(out);
        tail = out->tail;
        if (((head < tail) && (alloced_size <= tail + 1 - head))
                || ((head > tail) && (alloced_size <= SHARED_SIZE - head))) {
            alloced_start = head;
        } else if ((head > tail) && (alloced_size <= tail + 1)) {
            wrapped = true;
            alloced_start = 0;
        } else if (head > tail) {
            cmpxchg(&out->head, head, 0);
            Message *sentinel = CHANNEL_SHARED(out, head);
            sentinel->size = ~0;
            sentinel->alloced = SHARED_SIZE - head;
            sentinel->ready = true;
            return NULL;
        } else {
            return NULL;
        }
    }
#else // NO_ALLOCATOR
    alloced_start = 0;
#endif // NO_ALLOCATOR

    new_head = alloced_start + alloced_size;
    // detect if buffer is full and store info by when the head passes tail
    if (new_head == tail + 1) {
        // if this cmpxchg fails that just means someone else is garbage
        // collecting or flagging buffer as full, which is fine... might mean
        // some lost space or incorrect allocation failures but that is
        // tolerable
        // it'd be nice to reset head to zero, at least, but we can't do this
        // without updating next, and we don't want the receiver to miss
        // messages. it might be possible to do this iff next==head -nzb
        cmpxchg(&out->tail, tail, SHARED_SIZE);
    }
    if (unlikely(new_head >= SHARED_SIZE)) {
        new_head = 0;
    }

    if (!cmpxchg(&out->head, head, new_head)) {
        // XXX: this can only happen if we're sharing channels among processes
        // or threads and in both Pika and Hare this is the case for the
        // public channels we use to bootstrap
        // The code that bootstraps takes care of checking if the return value
        // of channel_allocate() is different from NULL so we can safely
        // comment out the following assertion -- siro
        // assert(false);
        return NULL;
    }

    if (unlikely(wrapped)) {
        Message *sentinel = CHANNEL_SHARED(out, head);
        sentinel->size = ~0;
        sentinel->alloced = SHARED_SIZE - head;
        sentinel->ready = true;
    }

    // For some reason, moving this initialization above the cmpxchg is a
    // significant performance improvement. Doing so leads to a race condition,
    // so it can only be done for single threads. However, changing cmpxchg to
    // a simple assignment inexplicably negates the advantage. There must be
    // something going on with caching, but I can't imagine what it is -nzb
#ifdef CHANNEL_USE_SCRATCH
    Message *alloced = CHANNEL_SCRATCH(out, alloced_start);
    alloced->size = size;
#else
    Message *alloced = CHANNEL_SHARED(out, alloced_start);
    alloced->size = 0;
#endif
    alloced->ready = true;
    alloced->alloced = alloced_size;

    // prefetch where we will allocate the next message
#ifdef CHANNEL_PREFETCH
    __builtin_prefetch(CHANNEL_SHARED(out, new_head), 1, 3);
#endif

    return (void*)&alloced[1];
}

    static inline void
channel_free(Channel *chan __attribute__ ((unused)), void *data)
{
    barrier();

    /* free all cache lines in the message to prevent false positives
     * receiving messages after wrapping around */
    Message *message = CHANNEL_MSG_FROM_PAYLOAD(data);
    Message *end = CHANNEL_MSG_FORTH(message, message->alloced);
    // FIXME: this cycle should be reverted to avoid a race with
    // channel_allocate -- siro
    while (message < end) {
        message->ready = false;
        message = CHANNEL_MSG_FORTH(message, CACHELINE_SIZE);
    }

#ifdef CHANNEL_SFENCE_FREE
    wmb();
#endif
}

    static inline void
channel_send(Channel *chan __attribute__ ((unused)), void *data, size_t size)
{
    barrier();
    Message *message = CHANNEL_MSG_FROM_PAYLOAD(data);

#ifdef CHANNEL_DEBUG
    fprintf(stderr, "%d: send %lu bytes @ %lu (%p), chan {%lu,%lu,%lu}, msg 0x%lx -> {%u,%u,%d}\n", (int)syscall(SYS_gettid), size, (uint8_t*)data-(uint8_t*)CHANNEL_SHARED(chan->out,0), data, chan->out->head, chan->out->tail, chan->out->next, *(uintptr_t*)message, message->size, message->alloced, message->ready);
#endif  /* DEBUG */

#ifdef CHANNEL_USE_SCRATCH
    uint8_t *src = (uint8_t *) message;
    size_t msg_start = (uint8_t *)src - (uint8_t *)CHANNEL_SCRATCH(chan, 0);
    uint8_t *dest = (uint8_t *) CHANNEL_SHARED(chan, msg_start);
    _channel_memcpy_cache_aligned(dest, src, sizeof(Message) + size);
#else
    message->size = size;
#endif // scratch

#ifdef CHANNEL_SFENCE_SEND
#ifdef CHANNEL_USE_SSE
    _mm_sfence();
#else
    wmb();
#endif // SSE
#endif // fence
}

    static inline size_t
__channel_poll(Channel * chan, HalfChannel *& in, size_t & next, Message *& msg)
{
    in = chan->in;
    next = in->next;

retry:
    msg = CHANNEL_SHARED(in, next);
#ifndef CHANNEL_USE_SCRATCH
    if (CHANNEL_MSG_READY(msg, false) || msg->size == 0)
#else
        if (CHANNEL_MSG_READY(msg, false))
#endif
        {
            if (*chan->closed) {
                return -1;
            } else {
                return 0;
            }
        }

    if (unlikely(msg->size == (uint16_t) ~0)) {
        if (!cmpxchg(&in->next, next, 0))
            return 0;
        msg->ready = false;
        next = 0;
        goto retry;
    }

    return msg->size;
}

    static inline size_t
channel_poll(Channel *chan)
{
    assert(chan != NULL);
    assert(chan->in != NULL);

    HalfChannel *in;
    size_t next;
    Message *msg;

    return __channel_poll(chan, in, next, msg);
}

static inline size_t
channel_multi_poll(Channel **chans, int nchans, uintptr_t stride)
{
    assert(chans != NULL || nchans == 0);

    if (stride == 0) {
        stride = sizeof(Channel);
    }

    int i = 0;
    size_t size;
    int count = 0;
    for (i = 0; i < nchans; ++i) {
        size = channel_poll(*((Channel **) ((uintptr_t) chans + i * stride)));
        if (size > 0)
            count++;
    }

    return count;
}

    static inline size_t
channel_receive(Channel *chan, void **data)
{
    assert(data != NULL);
    *data = NULL;

    HalfChannel *in;
    size_t next;
    Message *msg;

    size_t size;
    size = __channel_poll(chan, in, next, msg);

    if (size == (size_t) -1 || size == 0)
        return size;

#ifndef CHANNEL_NO_ALLOCATOR
    size_t new_next;

    new_next = next + msg->alloced;
    if (unlikely(new_next >= SHARED_SIZE))
        new_next = 0;

    if (!cmpxchg(&in->next, next, new_next)) {
        return 0;
    }
#endif

#ifdef CHANNEL_DEBUG
    if (chan->out) {
        fprintf(stderr, "%d: rcvd %lu bytes @ %lu (%p), chan {%lu,%lu,%lu}, msg 0x%lx -> {%u,%u,%d}, next %lu -> %lu\n", (int)syscall(SYS_gettid), size, (uint8_t*)&msg[1]-(uint8_t*)CHANNEL_SHARED(in,0), (void*)&msg[1], in->head, in->tail, in->next, *(uintptr_t*)msg, msg->size, msg->alloced, msg->ready, next, new_next);

    }
#endif  /* DEBUG */

    *data = (void *) &msg[1];
    return size;
}

    static inline size_t
channel_receive_blocking(Channel *chan, void **data)
{
    size_t size;
    do {
        size = channel_receive(chan, data);
        if(size == 0)
            sched_yield();
    } while (size == 0);
    return size;
}

// Receive a message from multiple channels.
//
// @param [in] chans A list of channels to check.
// @param [in] nchans The number of channels in the list.
// @param [in] Stride between channels in the list. If 0 assumed to be
// sizeof(HalfChannel).
// @param[in] blocking True if this should wait indefinitely for
// messages to arrive on any of the channels.
// @param [out] data Pointer to received message's payload.
// @param [out] data Size of received message.
// @param [inout] Last position in the list of channels. Can pass in same
// parameter to multiple calls to fairly check across channels. Also lets caller
// know where to free received message. Pass in -1 to first call.
    static inline size_t
channel_select(Channel **chans, int nchans, uintptr_t stride, bool blocking,
        void **data, int *pos)
{
    assert(chans != NULL || nchans == 0);
    assert(data != NULL);
    assert(pos);

    if (nchans == 0) {
        *pos = -1;
        return false;
    }

    if (stride == 0) {
        stride = sizeof(Channel);
    }

    int i = *pos + 1;

    if (i < 0 || i >= nchans) {
        i = 0;
    }
    do {
        int j;
        size_t size;

        for (j = i; j < nchans; ++j) {
            size = channel_receive(*((Channel **) ((uintptr_t) chans + j * stride)), data);
            if (size != 0) {
                *pos = j;
                return size;
            }
        }
        for (j = 0; j < i; ++j) {
            size = channel_receive(*((Channel **) ((uintptr_t) chans + j * stride)), data);
            if (size != 0) {
                *pos = j;
                return size;
            }
        }
    } while (blocking);

    return 0;
}

