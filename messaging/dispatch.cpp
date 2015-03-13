#include <messaging/dispatch.h>
#include <messaging/dispatch_reset.h>
#include <common/pool.hpp>
#include <common/circular_queue.hpp>
#include <common/common.h>
#include <common/map.hpp>

#include <stdio.h>
#include <vector>
#include <deque>
#include <tr1/unordered_map>
#include <iostream>
#include <iomanip>
#include <config/config.h>

using namespace std;
using namespace tr1;

  /* Private */

static const size_t MAX_CHANNELS = CONFIG_CHANNEL_POOL_SIZE;
static const size_t MAX_PENDING_REQUESTS = 64;
static const size_t MAX_TRANSACTIONS = 1024;
static const size_t MAX_HANDLERS = 128;

typedef struct {
    void *data;
    size_t size;
    Channel *from;
} PendingMessage;

typedef unordered_map<Address, Channel *> ChannelMap;
typedef Map<int, PendingMessage *, MAX_TRANSACTIONS> PendingMessageMap;
typedef Map<int, Thread *, MAX_TRANSACTIONS> ThreadMap;
typedef unordered_map<DispatchHandlerType, DispatchHandler> HandlerMap;

struct Dispatcher {
    vector<Endpoint *> request_endpoints;
    Endpoint *response_endpoint;
    vector<Channel *> channels;
    ChannelMap openned_channels;

    HandlerMap handlers;
    ThreadTarget idle_target;
    void * idle_arg;

    CircularQueue<PendingMessage *, MAX_PENDING_REQUESTS> pending_requests;
    CircularQueue<PendingMessage *, MAX_TRANSACTIONS> pending_responses;
    PendingMessageMap pending_response_map;
    Pool<PendingMessage, MAX_TRANSACTIONS> pending_pool;
    PendingMessage * next_response; // used to pass responses to waked threads quickly out of band -nzb

    ThreadMap threads;

    bool running;
    DispatchTransaction active;

    Dispatcher(): response_endpoint(NULL), idle_target(NULL), idle_arg(NULL), next_response(NULL)
    {
        channels.reserve(MAX_CHANNELS);
    }

    ~Dispatcher() {
        free_resources();
    }

    //The dispatcher will keep track of a bunch of channels
    //that it has open. Therefore, in order to not leak memory
    //(especially on the remote end) we need to ensure that the
    //dispatcher is freed -- otherwise the channels won't get
    //closed (and reclaimed)
    //
    //As a result, in some situations (application calling _exit() for example)
    //we have to force-close the channels. In this situation we will call the
    //dispatcher.free_resources() function as the destructor will not get called.
    //In the normal case, the distructor for global objects always gets called
    //and thus we free the channels -- so we have to be able to call the function
    //twice. --cg3
    void free_resources()
    {
        dispatchCheckNotifications();
        for (vector<Channel *>::iterator i = channels.begin();
             i != channels.end();
             i++) {
            channel_close(*i);
        }
        channels.clear();
        if (response_endpoint != NULL) {
            endpoint_destroy(response_endpoint);
            response_endpoint = NULL;
        }
        running = false;
    }
};

static void (*_dispatchCloseChannelCallbackFunc)(Channel *) = NULL;
static void (*_dispatchNotifyCallbackFunc)(void *, size_t) = NULL;

static Dispatcher dispatcher;

static void _dispatchMain(void *);

void dispatchSetCloseCallback(void (*callback)(Channel *))
{
    _dispatchCloseChannelCallbackFunc = callback;
}

void dispatchSetNotifyCallback(void (*callback)(void *, size_t))
{
    _dispatchNotifyCallbackFunc = callback;
}

static void _dispatchCloseChannelCallback(size_t pos)
{
    if(_dispatchCloseChannelCallbackFunc)
        _dispatchCloseChannelCallbackFunc(dispatcher.channels[pos]);
}

static void _dispatchCloseChannel(size_t pos) {
    // channel closed
    Channel * from = dispatcher.channels.back();
    Channel * to = dispatcher.channels[pos];
    channel_close(to);
    *to = *from;
    dispatcher.channels.pop_back();

    // remap existing pointers -- this should happen
    // infrequently so iterating should be acceptable
    for (ChannelMap::iterator i = dispatcher.openned_channels.begin();
         i != dispatcher.openned_channels.end();
         i++) {
        if (i->second == from) {
            i->second = to;
        }
    }
    for (size_t i = 0; i < dispatcher.pending_requests.size(); i++) {
        PendingMessage * pmsg = dispatcher.pending_requests.at(i);
        if (pmsg->from == from) {
            pmsg->from = to;
        }
    }
    if (0) {
        cerr << "[" << getpid() << "] #channels = " << dispatcher.channels.size() << endl;
        for (auto& i : dispatcher.channels) {
            cerr << "[" << getpid() << "]"
                << " local = " << setw(16) << hex << ((Endpoint *) (i->ep))->local_ep_addr
                << " remote = " << setw(16) << hex << i->remote_ep_addr
                << dec << endl;
        }
    }
}

// Process a valid incoming message, add it to the appropriate queues,
// etc..
static inline bool _dispatchProcessMessage(PendingMessage * pmsg) {
    _DispatchHeader *hdr = (_DispatchHeader *) pmsg->data;
    if (hdr->header.handler == -1) {
        // response
        __attribute__((unused)) bool pushed;
        pushed = dispatcher.pending_responses.push(pmsg);
        assert(pushed);
        return false;
    } else if(hdr->header.handler == -2)
    {
        // notification
        if(_dispatchNotifyCallbackFunc)
            _dispatchNotifyCallbackFunc(&hdr[1], pmsg->size - sizeof(_DispatchHeader));
        hdr->from = pmsg->from;
        dispatchFree(hdr->header.transaction, &hdr[1]);
        dispatcher.pending_pool.free(pmsg);
        return true;
    } else {
        // request
        assert(dispatchIsRunning());
        dispatcher.pending_requests.push(pmsg);
        return false;
    }
}

static inline void _dispatchAcceptChannels() {
    if (unlikely(dispatcher.channels.size() == MAX_CHANNELS)) {
        // can't resize vector because that would invalidate
        // existing pointers. this should be resolved with garbage
        // collection of unused channels and remapping pointers,
        // but this is deferred
        warning("Can't accept new channels, we're full!");
        assert(false);
        return;
    }

    for (size_t i = 0; i < dispatcher.request_endpoints.size(); ) {
        Channel * chan = endpoint_accept(dispatcher.request_endpoints[i]);
        if (chan == NULL) {
            ++i;
            continue;
        }

        // todo: set ref count (used to be -1, sentinel value
        // indicating we didn't establish this connection so we
        // shouldn't garbage collect it, but that isn't necessary)
        dispatcher.channels.push_back(chan);
    }
}

// Check for incoming messages. If no messages are available, check
// for new channels.
static inline PendingMessage * _dispatchPoll(bool accept_channels) {
    while (unlikely(dispatcher.pending_requests.size() == dispatcher.pending_requests.capacity())) {
        // can't take the risk of pulling more messages off the queue;
        // need to run main loop
        threadSchedule(threadCreate(_dispatchMain, NULL));
        threadYield(true);
    }

    // we need to know WHICH channel a request arrived on so we can
    // pass the correct response channel in _dispatchMain, and that
    // requires using channel_select. requests will not arrive if we
    // aren't running dispatch, however, and in that case select
    // doesn't make sense anyway.
    static int pos = -1;
    static PendingMessage * pmsg;

    if (pmsg == NULL) {
        pmsg = dispatcher.pending_pool.allocate();
    }

    if(accept_channels)
        _dispatchAcceptChannels();

    pmsg->size = channel_select(&dispatcher.channels[0], dispatcher.channels.size(),
                                sizeof(dispatcher.channels[0]), false, &pmsg->data, &pos);
    pmsg->from = dispatcher.channels[pos];

    if (pmsg->size != 0) {
        if (pmsg->size == (size_t) -1) {
            _dispatchCloseChannelCallback(pos);
            _dispatchCloseChannel(pos);
        } else {
            PendingMessage * ret = pmsg;
            pmsg = NULL;
            return ret;
        }
    }

    return NULL;
}

int dispatchCheckQueues() {
    return channel_multi_poll(&dispatcher.channels[0], dispatcher.channels.size(),
                                sizeof(dispatcher.channels[0]));
}

static void _dispatchProcessRequests() {
    // requests
    PendingMessage * msg = dispatcher.pending_requests.front();
    dispatcher.pending_requests.pop();

    _DispatchHeader * hdr = (_DispatchHeader *) msg->data;
    DispatchHandlerType type = hdr->header.handler;

    HandlerMap::iterator it = dispatcher.handlers.find(type);
    if (it == dispatcher.handlers.end()) {
        cerr << "Couldn't find a handler for " << type << endl;
        return;
    }

    DispatchHandler handler = it->second;
    DispatchTransaction transaction = hdr->header.transaction;

    dispatcher.active = transaction;
    handler(msg->from, transaction, &hdr[1], msg->size - sizeof(_DispatchHeader));

    dispatcher.pending_pool.free(msg);
}

static void _dispatchProcessResponses() {
    // responses
    PendingMessage * msg = dispatcher.pending_responses.front();
    dispatcher.pending_responses.pop();

    _DispatchHeader * hdr = (_DispatchHeader *) msg->data;
    ThreadMap::Iterator thread_itr = dispatcher.threads.find(hdr->header.transaction.id);

    if (thread_itr != dispatcher.threads.end()) {
        // sleeping thread found
        Thread * thread = thread_itr->value;

        dispatcher.next_response = msg;
        dispatcher.threads.rm(thread_itr);

        threadSwitch(thread);
    } else {
        // no sleeping thread (message arrived before thread called receive())
        // todo: check that it arrived for some active transaction
        assert(dispatcher.pending_response_map.find(hdr->header.transaction.id)
               == dispatcher.pending_response_map.end());
        dispatcher.pending_response_map.insert(hdr->header.transaction.id, msg);
    }
}

static void _dispatchMain(void *) {
    bool accept_new = true;
main_again:
    if (dispatcher.pending_requests.size() > 0) {
        _dispatchProcessRequests();
    } else if (dispatcher.pending_responses.size() > 0) {
        _dispatchProcessResponses();
    } else {

        PendingMessage * pmsg = _dispatchPoll(accept_new);
        accept_new = false;
        if (pmsg) {
            hare_last();
            _dispatchProcessMessage(pmsg);
            goto main_again;
        }
        else if(ENABLE_MSG_YIELD) {
            hare_sleep();
        }

        if (likely(dispatcher.idle_target != NULL)) {
            dispatcher.idle_target(dispatcher.idle_arg);
        }

    }
}

void dispatchCheckNotifications()
{
    PendingMessage * pmsg;
    do
    {
        pmsg = _dispatchPoll(false);
        if (pmsg) {
            _dispatchProcessMessage(pmsg);
        }
    } while(pmsg);
}

static inline void _dispatchInit() {
}

  /* Implementation */
bool dispatchIsRunning() {
    return dispatcher.running && threadingIsRunning();
}

void dispatchStart() {
    assert(!dispatcher.running);
    _dispatchInit();
    dispatcher.running = true;
    threadingStart(_dispatchMain, NULL);
}

void dispatchStop() {
    dispatcher.running = false;
    threadingStop();
}

void dispatchListen(Endpoint * endpoint) {
    _dispatchInit();
    dispatcher.request_endpoints.push_back(endpoint);
}

void dispatchListenChannel(Channel * chan) {
    _dispatchInit();
    dispatcher.channels.push_back(chan);
}

void dispatchIgnore(Endpoint * endpoint) {
    _dispatchInit();
    vector<Endpoint *> & v = dispatcher.request_endpoints;

    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] == endpoint) {
            v[i] = v.back();
            v.pop_back();
            --i;
        }
    }
}

void dispatchIgnoreChannel(Channel * chan) {
    _dispatchInit();
    vector<Channel *> & v = dispatcher.channels;

    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] == chan) {
            v[i] = v.back();
            v.pop_back();
            --i;
        }
    }
}

void dispatchRegister(DispatchHandlerType type, DispatchHandler handler) {
    _dispatchInit();
    if (type != -1) {
        dispatcher.handlers[type] = handler;
    } else {
        error("Can't register handler type -1.");
    }
}

void dispatchUnregister(DispatchHandlerType type) {
    _dispatchInit();
    dispatcher.handlers.erase(type);
}

void dispatchRegisterIdle(ThreadTarget tgt, void * arg)
{
    dispatcher.idle_target = tgt;
    dispatcher.idle_arg = arg;
}

void dispatchUnregisterIdle()
{
    dispatcher.idle_target = NULL;
}

static inline bool _dispatchShouldYield() {
    if (unlikely(!dispatchIsRunning())) {
        return false;
    }

    if (threadingGetNumThreads() > 1) {
        return true;
    }

    if (dispatcher.pending_requests.size() > 0) {
        return true;
    }

    if (dispatcher.pending_responses.size() > 0 &&
        dispatcher.threads.size() > 0) {
        return true;
    }

    return false;
}

static inline PendingMessage * _dispatchCheckPendingResponseMap(DispatchTransaction trans) {
    // check if message arrived before calling receive()
    PendingMessageMap::Iterator imsg;

    imsg = dispatcher.pending_response_map.find(trans.id);

    if (imsg != dispatcher.pending_response_map.end()) {
        // message arrived before calling receive
        PendingMessage * pmsg = imsg->value;
        dispatcher.pending_response_map.rm(imsg);
        return pmsg;
    } else {
        return NULL;
    }
}

static inline PendingMessage * _dispatchWaitForMessage(DispatchTransaction trans) {
    // message isn't here yet, check for new messages and either spin
    // or sleep until our message arrives
    PendingMessage * pmsg;

    int other_response_count = 0;

    do {
        pmsg = _dispatchPoll(false);

        if (pmsg != NULL) {
            _DispatchHeader *hdr = (_DispatchHeader *) pmsg->data;
            if (hdr->header.handler == -1 && hdr->header.transaction.id == trans.id) {
                while(other_response_count--)
                    _dispatchProcessResponses();
                return pmsg;
            } else {
                bool processed = _dispatchProcessMessage(pmsg);
                if(!processed) other_response_count++;
            }
        }
        else if(ENABLE_MSG_YIELD)
        {
            hare_client_sleep();
            continue;
        }
    }
    while (!_dispatchShouldYield());

    return NULL;
}

static inline PendingMessage * _dispatchSleepUntilMessage(DispatchTransaction trans) {
    // go to sleep until our message arrives, which will be passed via
    // dispatcher.next_response
    PendingMessage * pmsg;
    dispatcher.threads.insert(trans.id, threadingGetCurrent());
    threadYield(false);

    assert(dispatcher.next_response != NULL);
    pmsg = dispatcher.next_response;
    dispatcher.next_response = NULL;
    dispatcher.active = trans;

    return pmsg;
}

size_t dispatchReceive(DispatchTransaction trans, void **data) {
    PendingMessage * pmsg;

    pmsg = _dispatchCheckPendingResponseMap(trans);

    if (pmsg == NULL) {
        pmsg = _dispatchWaitForMessage(trans);

        if (pmsg == NULL) {
            pmsg = _dispatchSleepUntilMessage(trans);
        }
    }

    assert(pmsg);
    _DispatchHeader *hdr = (_DispatchHeader *) pmsg->data;
    assert(hdr->header.handler == -1);
    assert(hdr->header.transaction.id == trans.id);

    *data = (void*) &hdr[1];
    size_t size = pmsg->size - sizeof(_DispatchHeader);

    hdr->from = pmsg->from;

    dispatcher.pending_pool.free(pmsg);

    return size;
}

Channel * dispatchOpen(Address peer) {
    _dispatchInit();

    if (dispatcher.response_endpoint == NULL) {
        dispatcher.response_endpoint = client_endpoint_create();
    }

    ChannelMap::iterator it = dispatcher.openned_channels.find(peer);
    if (it == dispatcher.openned_channels.end()) {
        /* create the channel */
        if (dispatcher.openned_channels.size() == MAX_CHANNELS) {
            // see comment in _dispatchPoll
            return NULL;
        }

        // fixme: is there a deadlock case here? -nzb
        Channel * chan = endpoint_connect(dispatcher.response_endpoint, peer);
        dispatcher.channels.push_back(chan);

        pair<ChannelMap::iterator, bool> inserted;
        inserted = dispatcher.openned_channels.insert(make_pair(peer, dispatcher.channels.back()));
        assert(inserted.second);
        return inserted.first->second;
    } else {
        // todo: ref count increment
        return it->second;
    }
}

void dispatchClose(Channel *chan) {
    channel_close(chan);
}

void dispatchRegisterResponseEndpoint(Endpoint *ep)
{
    assert(ep != NULL);
    dispatcher.response_endpoint = ep;
}

void dispatcherFree()
{
    dispatcher.free_resources();
}

void dispatchReset()
{
    dispatcher.request_endpoints.clear();
    dispatcher.response_endpoint = NULL;
    dispatcher.channels.clear();
    dispatcher.openned_channels.clear();
    dispatcher.idle_target = NULL;
    dispatcher.idle_arg = NULL;
    dispatcher.next_response = NULL;
}



bool g_sleep = true;
void nameserver_dontsleep()
{
    g_sleep = false;
}
