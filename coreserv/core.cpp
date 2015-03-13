#include <sched.h>
#include <common/param.h>
#include <config/config.h>
#include <iostream>
#include <messaging/dispatch.h>
#include <utilities/tsc.h>
#include <utmpx.h>

#include <core/core.h>
#include "core_internal.h"

Channel *core_chan = NULL;


uint64_t 
core_get(uint64_t group)
{
    if(core_chan == NULL)
    {
        //Address addr;
        //assert(name_lookup(CORE_SERVER_NAME, &addr));
        
        core_chan = dispatchOpen(CORE_SERVER_ADDR);
    }

    assert(core_chan != NULL);

    CoreGetRequest *req = (CoreGetRequest*) dispatchAllocate(core_chan, sizeof(CoreGetRequest));
    req->group = group;
    req->pid = getpid();

    DispatchTransaction trans;
    CoreGetResponse *resp;

    trans = dispatchRequest(core_chan, CORE_GET, req, sizeof(CoreGetRequest));
    __attribute__((unused))
    size_t size = dispatchReceive(trans, (void**) &resp);

    assert(size == sizeof(CoreGetResponse));

    uint64_t ret = resp->core;
    assert(ret != (uint64_t)-1);
    assert(resp->ack == 1);
    dispatchFree(trans, resp);

    return ret;
}

int bind_to_default_core()
{
    char *group = getenv("COREGROUP");
    uint64_t gnum;
    if(group)
        gnum = atol(group);
    else
        gnum = 0;

    return bind_to_core(core_get(gnum));
}


