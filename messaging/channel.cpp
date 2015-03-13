#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <list>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <tr1/unordered_map>
#include <unistd.h>
#include <utility>

#include <common/util.h>
#include <messaging/channel.h>

#define DOUBLE_MMAP 1

using namespace std;

enum ChannelOperation {
    CONNECT,
    DISCONNECT
};

typedef struct {
    enum ChannelOperation op;
    int ep_size;
    Address ep_addr;
} ChannelRequest;

typedef struct {
    Address ep_addr;
    uint64_t chan_off;
} ChannelResponse;

// XXX: I'm using a dedicated version of the -rdtsc64- function since this one
// does not put a memory fence before reading the timestamp counter -siro
static inline uint64_t
__addr_gen()
{
    uint32_t low_, high_;

    __asm__ __volatile__ ("rdtsc" : "=a" (low_), "=d" (high_));
    return (((uint64_t) high_ << 32) | (uint64_t) low_);
}

static inline void
__addr_create(Address *addr, uintptr_t *buf, size_t size, bool fixed)
{
    char ascii_addr[NAME_MAX + 1];
    int fd;
    if (!fixed) { // then generate an address for me (default)
retry:
        *addr = __addr_gen();
        snprintf(ascii_addr, NAME_MAX, SHARED_MEMORY_PREFIX "%llx", (unsigned long long) *addr);
        fd = shm_open(ascii_addr, O_RDWR | O_CREAT | O_EXCL | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (fd == -1 && errno == EEXIST)
            goto retry;
    } else {
        snprintf(ascii_addr, NAME_MAX, SHARED_MEMORY_PREFIX "%llx", (unsigned long long) *addr);
        shm_unlink(ascii_addr); // this may silently fail
        fd = shm_open(ascii_addr, O_RDWR | O_CREAT | O_EXCL | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    }
    assert(fd >= 0);
    int retval __attribute__((unused)) = ftruncate(fd, size);
    assert(retval == 0);
    *buf = (uintptr_t) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert((void *) *buf != MAP_FAILED);
    close(fd);
}

static inline void
__addr_destroy(Address addr, uintptr_t buf, size_t size)
{
    int retval __attribute__((unused)) = munmap((void *) buf, size);
    assert(retval == 0);
    char ascii_addr[NAME_MAX + 1];
    snprintf(ascii_addr, NAME_MAX, SHARED_MEMORY_PREFIX "%llx", (unsigned long long) addr);
    retval = shm_unlink(ascii_addr);
    assert(retval == 0);
}

static inline void
__addr_open(Address addr, uintptr_t *buf, size_t size, uint64_t offset)
{
    char ascii_addr[NAME_MAX + 1];
    snprintf(ascii_addr, NAME_MAX, SHARED_MEMORY_PREFIX "%llx", (unsigned long long) addr);
    int fd = shm_open(ascii_addr, O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    assert(fd >= 0);
    *buf = (uintptr_t) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
    if ((void *) *buf == MAP_FAILED)
        perror("mmap()");
    assert((void *) *buf != MAP_FAILED);
    close(fd);
    if (0)
        cerr << "[" << getpid() << "] " << __PRETTY_FUNCTION__ << ": address = " << ascii_addr << " buffer = " << hex << *buf << dec << " size = " << size << endl;
}

static inline void
__addr_close(uintptr_t buf, size_t size)
{
    if (0)
        cerr << "[" << getpid() << "] " << __PRETTY_FUNCTION__ << ": buffer = " << hex << buf << dec << " size = " << size << endl;
    int retval __attribute__ ((unused)) = munmap((void *) buf, size);
    assert(retval == 0);
}

static inline void
__channel_setup_owned(Channel *chan, Endpoint *ep, Address remote_ep_addr, uintptr_t buf)
{
    chan->owned = true;
    chan->ep = (void *) ep;
    chan->remote_ep_addr = remote_ep_addr;
    chan->closed = (bool *) buf;
    chan->in = (HalfChannel *) ((uintptr_t) chan->closed + 2 * CACHELINE_SIZE);
    chan->out = &chan->in[1];

    *chan->closed = false;

    chan->in->head = 0;
    chan->in->tail = SHARED_SIZE - 1;
    chan->in->next = 0;

    chan->out->head = 0;
    chan->out->tail = SHARED_SIZE - 1;
    chan->out->next = 0;

    wmb();
}

static inline void
__channel_setup_non_owned(Channel *chan, Endpoint *ep, Address remote_ep_addr, uintptr_t buf)
{
    chan->owned = false;
    chan->ep = (void *) ep;
    chan->remote_ep_addr = remote_ep_addr;
    chan->closed = (bool *) buf;
    chan->out = (HalfChannel *) ((uintptr_t) chan->closed + 2 * CACHELINE_SIZE);
    chan->in = &chan->out[1];
}

static
Endpoint *
__endpoint_create(Address local_ep_addr, int size, bool fixed)
{
    Endpoint *ep = new Endpoint;
    assert(ep != NULL);
    ep->local_ep_addr = local_ep_addr;
    ep->ep_size = size;

    uintptr_t buf;

    __addr_create(&ep->local_ep_addr, &buf, ENDPOINT_SIZE + size, fixed);
    ep->local_ep_buf = buf;
    for (int i = 0; i < ENDPOINT_PUBLIC_CHANNELS; ++i) {
        __channel_setup_owned(&ep->chan[i], ep, (Address) ~0, buf + CHANNEL_SIZE * i);
        ep->chanp[i] = &ep->chan[i];
    }
    __channel_setup_owned(&ep->side_chan, ep, (Address) ~0, buf + CHANNEL_SIZE * ENDPOINT_PUBLIC_CHANNELS);
    ep->local_chan_buf = buf + ENDPOINT_SIZE;
    for (buf = ep->local_chan_buf; buf < ep->local_chan_buf + size; buf += CHANNEL_SIZE)
        ep->local_chan_pool.push_back(buf);

    return ep;
}

Endpoint *
endpoint_create()
{
    return __endpoint_create((Address) ~0, MEMORY_POOL_SIZE, false);
}

Endpoint *
endpoint_create_fixed(Address local_ep_addr)
{
    return __endpoint_create(local_ep_addr, MEMORY_POOL_SIZE, true);
}

Endpoint *
client_endpoint_create()
{
    return __endpoint_create((Address) ~0, CLIENT_MEMORY_POOL_SIZE, false);
}

void
endpoint_destroy(Endpoint *ep)
{
    assert(ep != NULL);

    for (uintptr_t buf = ep->local_chan_buf; buf < ep->local_chan_buf + ep->ep_size; buf += CHANNEL_SIZE) {
        *((bool *) buf) = true;
        wmb();
    }
    for (tr1::unordered_map<Address, uintptr_t>::const_iterator i = ep->remote_ep_cache.begin(); i != ep->remote_ep_cache.end(); i++) {
        Channel remote_ep_chan;
        __channel_setup_non_owned(&remote_ep_chan, ep, i->first, i->second + CHANNEL_SIZE * (getpid() % ENDPOINT_PUBLIC_CHANNELS));
        ChannelRequest *chan_req;
        while ((chan_req = (ChannelRequest *) channel_allocate(&remote_ep_chan, sizeof(ChannelRequest))) == NULL)
            ;
        chan_req->op = DISCONNECT;
        chan_req->ep_addr = ep->local_ep_addr;
        chan_req->ep_size = ep->ep_size;
        channel_send(&remote_ep_chan, (void *) chan_req, sizeof(ChannelRequest));
    }
    // XXX: this is ugly but closed is the first flag inside the shared memory region... -- siro
    __addr_destroy(ep->local_ep_addr, (uintptr_t) ep->chan[0].closed, ENDPOINT_SIZE + ep->ep_size);
    delete ep;
}

Address
endpoint_get_address(const Endpoint *ep)
{
    assert(ep != NULL);

    return ep->local_ep_addr;
}

Channel *
endpoint_accept(Endpoint *ep)
{
    static int position = -1;

    assert(ep != NULL);

    // receive request or fail
    ChannelRequest *chan_req;
    size_t size = channel_select(ep->chanp, ENDPOINT_PUBLIC_CHANNELS, sizeof(Channel*), false, (void**) &chan_req, &position);
    if (size == 0)
        return NULL;
    assert(size == sizeof(ChannelRequest));
    // allocate new channel and fill in from request
    ChannelOperation op = chan_req->op;
    Address remote_ep_addr = chan_req->ep_addr;
    int ep_size = chan_req->ep_size;
    channel_free(&ep->chan[position], (void *) chan_req);

    if (op == CONNECT) {
        // lookup the Endpoint cache to see if we already talked with the remote
        // address
        Channel remote_ep_side_chan;
        uintptr_t buf;
        std::tr1::unordered_map<Address, uintptr_t>::const_iterator i = ep->remote_ep_cache.find(remote_ep_addr);
        if (i == ep->remote_ep_cache.end()) {
            __addr_open(remote_ep_addr, &buf, ENDPOINT_SIZE + ep_size, 0);
            ep->remote_ep_cache.insert(std::make_pair(remote_ep_addr, buf));
        } else {
            buf = i->second + CHANNEL_SIZE;
        }
        __channel_setup_non_owned(&remote_ep_side_chan, ep, remote_ep_addr, buf + CHANNEL_SIZE * ENDPOINT_PUBLIC_CHANNELS);

        buf = 0;
        for (std::list<uintptr_t>::iterator j = ep->local_chan_pool.begin(); j != ep->local_chan_pool.end(); ++j) {
            if (!((Channel *) *j)->closed) {
                buf = *j;
                j = ep->local_chan_pool.erase(j);
                break;
            }
        }
        assert(buf != 0);

        Channel* chan = new Channel;
        assert(chan != NULL);
        __channel_setup_owned(chan, ep, remote_ep_addr, buf);

        // tell peer about the Channel
        ChannelResponse *chan_res;
        while ((chan_res = (ChannelResponse *) channel_allocate(&remote_ep_side_chan, sizeof(ChannelResponse))) == NULL)
            ;
        chan_res->ep_addr = ep->local_ep_addr;
        chan_res->chan_off = buf - ep->local_ep_buf;
        channel_send(&remote_ep_side_chan, (void *) chan_res, sizeof(ChannelResponse));

        if (0)
          cerr << "[" << getpid() << "] #shm = " << ep->remote_ep_cache.size() << endl;

        return chan;
    } else if (op == DISCONNECT) {
        uintptr_t remote_ep_buf = ep->remote_ep_cache[remote_ep_addr];
        ep->remote_ep_cache.erase(remote_ep_addr);
        __addr_close(remote_ep_buf, ENDPOINT_SIZE + ep_size);

        if (0)
            cerr << "[" << getpid() << "] #shm = " << ep->remote_ep_cache.size() << endl;

        return NULL;
    }
    // XXX: wtf?! we shouldn't really be here -- siro
    assert(false);
    return NULL;
}

Channel *
endpoint_connect(Endpoint *ep, Address remote_ep_addr)
{
    assert(ep != NULL);

    int public_channel = getpid() % ENDPOINT_PUBLIC_CHANNELS;

    // lookup the Endpoint cache to see if we already talked with the remote
    // address
    Channel remote_ep_chan;
    uintptr_t buf;
    std::tr1::unordered_map<Address, uintptr_t>::const_iterator i = ep->remote_ep_cache.find(remote_ep_addr);
    if (i == ep->remote_ep_cache.end()) {
#if DOUBLE_MMAP
        __addr_open(remote_ep_addr, &buf, ENDPOINT_SIZE, 0);
#else
        __addr_open(remote_ep_addr, &buf, ENDPOINT_SIZE + MEMORY_POOL_SIZE, 0);
#endif
        ep->remote_ep_cache.insert(std::make_pair(remote_ep_addr, buf));
    } else {
        buf = i->second;
    }
    __channel_setup_non_owned(&remote_ep_chan, ep, remote_ep_addr, buf + CHANNEL_SIZE * (public_channel));

    // generate request to peer
    ChannelRequest *chan_req;
    while ((chan_req = (ChannelRequest *) channel_allocate(&remote_ep_chan, sizeof(ChannelRequest))) == NULL)
        sched_yield();
    chan_req->op = CONNECT;
    chan_req->ep_addr = ep->local_ep_addr;
    chan_req->ep_size = ep->ep_size;
    channel_send(&remote_ep_chan, (void *) chan_req, sizeof(ChannelRequest));

    // clean up and fill in channel structure with response
    ChannelResponse *chan_res;
    size_t size __attribute__((unused)) = channel_receive_blocking(&ep->side_chan, (void **) &chan_res);
    assert(size == sizeof(ChannelResponse) || size == (size_t) -1);
#if 0
    if (size != sizeof(ChannelResponse)) {
        if (size > 0)
            channel_free(&ep->side_chan, chan_res);
        return NULL;
    }
#endif
    assert(chan_res->ep_addr == remote_ep_addr);
    uintptr_t remote_chan_off = chan_res->chan_off;
    channel_free(&ep->side_chan, (void *) chan_res);

    Channel* chan = new Channel;
    assert(chan != NULL);

#if DOUBLE_MMAP
    int remain = remote_chan_off % 4096;
    __addr_open(remote_ep_addr, &buf, CHANNEL_SIZE + remain, remote_chan_off - remain);
#else
    int remain = remote_chan_off;
#endif
    __channel_setup_non_owned(chan, ep, remote_ep_addr, buf + remain);

    return chan;
}

Address
channel_get_local_address(const Channel *chan)
{
    return ((Endpoint *) chan->ep)->local_ep_addr;
}

Address
channel_get_remote_address(const Channel *chan)
{
    return chan->remote_ep_addr;
}

void
channel_close(Channel *chan)
{
    assert(chan != NULL);

    if (chan->owned) {
        Endpoint *ep = (Endpoint *) chan->ep;
        cmpxchg(chan->closed, false, true);
        ep->local_chan_pool.push_front((uintptr_t) chan->closed);
    } else {
        if (!cmpxchg(chan->closed, false, true))
            *chan->closed = false;
    }
    // delete chan;
}

