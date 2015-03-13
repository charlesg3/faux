#include <cassert>
#include <climits>
#include <common/util.h>
#include <config/config.h>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <list>
#include <messaging/dispatch.h>
#include <name/name.h>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unordered_map>
#include <unistd.h>
#include <core/core.h>

#include "name_internal.h"

static void sigint_handler(int signum);

typedef std::list<Address> EndpointAddresses;
typedef std::unordered_map<Name, EndpointAddresses> ForwardNameTable;

static Endpoint* ep;
static ForwardNameTable sock_name_to_addrs[SOCK_NUM];

static void nameserver_init();
static void nameserver_cleanup();
void nameserver_name_register(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
void nameserver_name_unregister(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
void nameserver_name_lookup(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
void nameserver_name_close(Channel* chan, DispatchTransaction trans, void* msg, size_t size);

int
main()
{
    int ret;
#if ENABLE_SCHEDULER_RR
    struct sched_param params;
    params.sched_priority = 1;
    ret = sched_setscheduler(0, SCHED_RR, &params);
    if(ret != 0)
        fprintf(stderr, "error setting scheduler.\n");
#endif
    ret = bind_to_core(CONFIG_SERVER_COUNT - 1);
    assert(ret == 0);
    signal(SIGINT, sigint_handler);
    nameserver_init();
    std::cout << "nameserver: started." << std::endl;
    dispatchStart();
    nameserver_cleanup();
    std::cout << "nameserver: stop" << std::endl;
    return 0;
}

static void
sigint_handler(int signum)
{
    dispatchStop();
}

static void idle(void * ignored)
{
    usleep(5000);
}

extern void nameserver_dontsleep();

static void
nameserver_init()
{
    nameserver_dontsleep();
    for (int sock_i = 0; sock_i < SOCK_NUM; ++sock_i)
        sock_name_to_addrs[sock_i].reserve(400);

    ep = endpoint_create_fixed(CONFIG_NAMESERVER_EP_ADDR);

    dispatchListen(ep);
    dispatchRegisterIdle(idle, NULL);
    dispatchRegister(NAME_REGISTER, nameserver_name_register);
    dispatchRegister(NAME_UNREGISTER, nameserver_name_unregister);
    dispatchRegister(NAME_LOOKUP, nameserver_name_lookup);
    dispatchRegister(NAME_CLOSE, nameserver_name_close);
}

static void
nameserver_cleanup()
{
    assert(ep != NULL);

    dispatchUnregister(NAME_CLOSE);
    dispatchUnregister(NAME_LOOKUP);
    dispatchUnregister(NAME_UNREGISTER);
    dispatchUnregister(NAME_REGISTER);
    dispatchIgnore(ep);

    endpoint_destroy(ep);
}

// FIXME: security issue in the nameserver
// The current implementation of the nameserver suffers of a notable security
// issue. In fact, given a endpoint address and the name under which it is
// registered, everyone can unregister the <name, address> pair from the
// nameserver.
// A secure implementation should avoid this problem allowing only the owner
// of the endpoint to unregister the <name, address> pair -siro
void
nameserver_name_register(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
{
    assert(msg != NULL);
    assert(size == sizeof(NameRegisterRequest));

    NameRegisterRequest* req = (NameRegisterRequest*) msg;
    int core = req->core;
    Name name = req->name;
    Address ep_addr = req->ep_addr;
    dispatchFree(trans, msg);

    int sock = CORE_TO_SOCK(core);
    ForwardNameTable::iterator i = sock_name_to_addrs[sock].find(name);
    if (i != sock_name_to_addrs[sock].end()) {
        i->second.push_front(ep_addr);
    } else {
        std::pair<Name, EndpointAddresses> name_to_addrs;
        name_to_addrs.first = name;
        name_to_addrs.second.push_front(ep_addr);
        sock_name_to_addrs[sock].insert(name_to_addrs);
    }

    NameRegisterResponse* res = (NameRegisterResponse*) dispatchAllocate(chan, sizeof(NameRegisterResponse));
    res->ack = true;
    dispatchRespond(chan, trans, res, sizeof(NameRegisterResponse));
}

void
nameserver_name_unregister(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
{
    assert(msg != NULL);
    assert(size == sizeof(NameUnregisterRequest));

    NameUnregisterRequest* req = (NameUnregisterRequest*) msg;
    int core = req->core;
    Name name = req->name;
    Address ep_addr = req->ep_addr;
    dispatchFree(trans, msg);

    int sock = CORE_TO_SOCK(core);
    ForwardNameTable::iterator i = sock_name_to_addrs[sock].find(name);
    if (i != sock_name_to_addrs[sock].end()) {
        i->second.remove(ep_addr);
        if (i->second.size() == 0)
            sock_name_to_addrs[sock].erase(i);
    }

    NameUnregisterResponse* res = (NameUnregisterResponse*) dispatchAllocate(chan, sizeof(NameUnregisterResponse));
    res->ack = true;
    dispatchRespond(chan, trans, res, sizeof(NameUnregisterResponse));
}

void
nameserver_name_lookup(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
{
    assert(msg != NULL);
    assert(size == sizeof(NameLookupRequest));

    NameLookupRequest* req = (NameLookupRequest*) msg;
    int core = req->core;
    Name name = req->name;
    dispatchFree(trans, msg);

    int sock = CORE_TO_SOCK(core);
    ForwardNameTable::iterator i = sock_name_to_addrs[sock].find(name);
    Address ep_addr = (Address) ~0;
    if (i != sock_name_to_addrs[sock].end()) {
        ep_addr = i->second.front();
        // this piece of code implement a somewhat unoptimized round-robin
        // among endpoints registered under the same name. In any case, the use
        // of a deque should allow fast pop from front and push to back -siro
        i->second.pop_front();
        i->second.push_back(ep_addr);
    }
    if (ep_addr == (Address) ~0) {
        for (int sock_i = 0; sock_i < SOCK_NUM; ++sock_i) {
            if (sock_i == sock)
                continue;
            i = sock_name_to_addrs[sock_i].find(name);
            if (i != sock_name_to_addrs[sock_i].end()) {
                ep_addr = i->second.front();
                // this piece of code implement a somewhat unoptimized round-robin
                // among endpoints registered under the same name. In any case, the use
                // of a deque should allow fast pop from front and push to back -siro
                i->second.pop_front();
                i->second.push_back(ep_addr);
                break;
            }
        }
    }

    NameLookupResponse* res = (NameLookupResponse*) dispatchAllocate(chan, sizeof(NameLookupResponse));
    res->ep_addr = ep_addr;
    res->ack = ep_addr != (Address) ~0 ? true : false;
    dispatchRespond(chan, trans, res, sizeof(NameLookupResponse));
}

void
nameserver_name_close(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
{
    assert(msg != NULL);
    assert(size == sizeof(NameCloseNotification));

    dispatchFree(trans, msg);
}

