#pragma once
#include <common/param.h>
#include <common/util.h>
#include <config/config.h>
#include <sys/sched/lib.h>

// Constants

#define SHARED_SIZE (CONFIG_CHANNEL_BUF_SIZE)

// Private Constants
#define CHANNEL_SIZE (2 * CACHELINE_SIZE + 2 * sizeof(HalfChannel))
#define SHARED_MEMORY_PREFIX "/faux-shm-"
#define ENDPOINT_SIZE (CHANNEL_SIZE * (ENDPOINT_PUBLIC_CHANNELS + 1))
#define MEMORY_POOL_SIZE (CHANNEL_SIZE * CONFIG_CHANNEL_POOL_SIZE)
#define CLIENT_MEMORY_POOL_SIZE (CHANNEL_SIZE * CONFIG_CLIENT_CHANNEL_POOL_SIZE)

#define ENDPOINT_PUBLIC_CHANNELS (1)
#if ENDPOINT_PUBLIC_CHANNELS < 1
#error "The Endpoint cannot have less then 1 public Channel"
#endif

// Structures

typedef struct {
    uint16_t size; // size of payload
    uint16_t alloced; // allocated message size (cache-aligned, with header)
    bool ready; // true if message is ready for receiver or receiver is still processing message (set => receiver owns message)
    bool dummy; // written to pull cache line into exclusive, not used. -nzb
    uint16_t __padding__;
} __attribute__((packed)) Message;

typedef size_t aligned_size_t __attribute__((aligned (2 * CACHELINE_SIZE)));

typedef struct {
    aligned_size_t head; // owned by sender - the first free byte in the buffer
    aligned_size_t tail; // owned by sender - the first allocated byte in the buffer, or SHARED_SIZE if the buffer is full
    aligned_size_t next; // owned by receiver - the next unreceived message
    uint8_t shared[SHARED_SIZE] __attribute__((aligned(CACHELINE_SIZE))); // shared buffer between sender & receiver
#ifdef CHANNEL_USE_SCRATCH
    uint8_t scratch[SHARED_SIZE] __attribute__((aligned(CACHELINE_SIZE))); // for constructing messages before sending
#endif
} HalfChannel;

typedef uint64_t Address;


typedef struct {
    bool owned;
    void *ep;
    Address remote_ep_addr;
    bool *closed;
    HalfChannel *in;
    HalfChannel *out;
} Channel;

#define __nr_HARE 351

static inline void enable_pcid()
{
#if ENABLE_PCIDE
    syscall(__nr_HARE,8);
#endif
}

static inline void hare_last()
{
    fosClientLastMessage();
}

static inline uint64_t hare_last_msg()
{
    return fosClientLastMessageTime();
}

static inline void hare_client_sleep()
{
    fosClientSleep(); //sched_sleep();
}

extern bool g_sleep;
void nameserver_dontsleep();

static inline void hare_sleep()
{
    if(!g_sleep)
        return;

    fosServerSleep();

}

static inline void hare_wake(int server_id)
{
#if ENABLE_HARE_WAKE
    syscall(__nr_HARE,2,server_id);
#endif
}

