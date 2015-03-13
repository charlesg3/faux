#include <common/param.h>
#include <config/config.h>
#include <iostream>
#include <messaging/dispatch.h>
#include <name/name.h>
#include <unistd.h>
#include <common/util.h>
#include <utmpx.h>

#include "name_internal.h"

static int g_core;
static Channel *g_name_chan;

// this controls whether the name-server round-robins servers
// on a per-chip basis or on a global basis
#define USE_LOCALITY 0

void
name_init()
{
    g_core = USE_LOCALITY ? sched_getcpu() : 0;
    g_name_chan = dispatchOpen(CONFIG_NAMESERVER_EP_ADDR);
}

void
name_cleanup()
{
    assert(g_name_chan != NULL);

    NameCloseNotification* notification = (NameCloseNotification*) dispatchAllocate(g_name_chan, sizeof(NameCloseNotification));
    dispatchRequest(g_name_chan, NAME_CLOSE, notification, sizeof(NameCloseNotification));
    dispatchClose(g_name_chan); // this function call does nothing -siro
    // channel_close(g_name_chan);
}

void
name_register(Name name, Address ep_addr)
{
    if (g_name_chan == NULL)
        name_init();

    NameRegisterRequest* req;
    DispatchTransaction trans;
    NameRegisterResponse* res;
    __attribute__((unused)) size_t size;

    req = (NameRegisterRequest*) dispatchAllocate(g_name_chan, sizeof(NameRegisterRequest));
    req->core = g_core;
    req->name = name;
    req->ep_addr = ep_addr;
    trans = dispatchRequest(g_name_chan, NAME_REGISTER, req, sizeof(NameRegisterRequest));
    hare_wake(39);
    size = dispatchReceive(trans, (void **) &res);
    assert(size == sizeof(NameRegisterResponse));
    assert(res->ack == true);
    dispatchFree(trans, res);
}

void
name_unregister(Name name, Address ep_addr)
{
    if (g_name_chan == NULL)
        name_init();

    NameUnregisterRequest* req;
    DispatchTransaction trans;
    NameUnregisterResponse* res;
    __attribute__((unused)) size_t size;

    req = (NameUnregisterRequest*) dispatchAllocate(g_name_chan, sizeof(NameUnregisterRequest));
    req->core = g_core;
    req->name = name;
    req->ep_addr = ep_addr;
    trans = dispatchRequest(g_name_chan, NAME_UNREGISTER, req, sizeof(NameUnregisterRequest));
    size = dispatchReceive(trans, (void **) &res);
    assert(size == sizeof(NameUnregisterResponse));
    assert(res->ack == true);
    dispatchFree(trans, res);
}

bool
name_lookup(Name name, Address* ep_addr)
{
    if (g_name_chan == NULL)
        name_init();

    NameLookupRequest* req;
    DispatchTransaction trans;
    NameLookupResponse* res;
    __attribute__((unused)) size_t size;
    bool ack;

    req = (NameLookupRequest*) dispatchAllocate(g_name_chan, sizeof(NameLookupRequest));
    req->core = g_core;
    req->name = name;
    trans = dispatchRequest(g_name_chan, NAME_LOOKUP, req, sizeof(NameLookupRequest));
    hare_wake(39);
    size = dispatchReceive(trans, (void **) &res);
    assert(size == sizeof(NameLookupResponse));
    *ep_addr = res->ep_addr;
    ack = res->ack;
    dispatchFree(trans, res);
    return ack;
}

void
name_reset(void)
{
    g_name_chan = NULL;
}

