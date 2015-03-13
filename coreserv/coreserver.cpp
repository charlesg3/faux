#include <cassert>
#include <climits>
#include <common/util.h>
#include <config/config.h>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <messaging/channel.h>
#include <messaging/dispatch.h>
#include <name/name.h>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unordered_map>
#include <unistd.h>

#include <name/name.h>
#include <core/core.h>

#include <sys/wait.h>

#include <common/param.h>

#include "core_internal.h"


#define CONFIG_CORESERV_GROUPS SOCK_NUM
#define CONFIG_CORESERV_CORES CORE_NUM

typedef std::list<uint64_t> CoreList;
typedef std::map<uint64_t, CoreList> CoreGroups;

typedef std::map<pid_t, CoreList> PidCores;

CoreGroups g_coregroup;
PidCores g_pidcore;
std::set<pid_t> g_pollpids;

Endpoint *ep;


void coreserver_core_get(Channel* chan, DispatchTransaction trans, void* msg, size_t size);

void coreserver_init();

void coreserver_idleloop(void*);

int
main()
{
    assert(bind_to_core(0) == 0);
    coreserver_init();

    ep = endpoint_create_fixed(CORE_SERVER_ADDR);

 // name_init();
 // name_register(CORE_SERVER_NAME, endpoint_get_address(endp));

    dispatchListen(ep);
    dispatchRegister(CORE_GET, coreserver_core_get);

    dispatchRegisterIdle(coreserver_idleloop, NULL);

    printf("coreserver: started\n");
    fflush(stdout);

    dispatchStart();

    return 0;
}


void coreserver_init()
{
    for(int i=0; i<CONFIG_CORESERV_GROUPS; i++)
    {
        g_coregroup[i] = CoreList();
    }

    // Start at 1, since core 0 is used for bootstrap services
    for(int i=1; i<CONFIG_CORESERV_CORES; i++)
        g_coregroup[i % CONFIG_CORESERV_GROUPS].push_back((uint64_t)i);

}

void coreserver_core_get(Channel* chan, DispatchTransaction trans, void *msg, size_t size)
{
    assert(size == sizeof(CoreGetRequest));

    CoreGetRequest *req = (CoreGetRequest*)msg;

    uint64_t core = -1;
    bool ack = 0;
    uint64_t group = req->group;

    // Default group
    if(group >= CONFIG_CORESERV_GROUPS)
        group = 0;

    uint64_t startgroup = group;
    do
    {
        CoreList &cores = g_coregroup[group];
        if(cores.size() != 0)
        {
            core = cores.front();
            ack = 1;
            cores.pop_front();

            if(g_pidcore.find(req->pid) == g_pidcore.end())
                g_pidcore[req->pid] = CoreList();

            CoreList &pcores = g_pidcore[req->pid];

            pcores.push_back(core);
            g_pollpids.insert(req->pid);

            break;
        }
        else
        {
            group = (group + 1) % CONFIG_CORESERV_GROUPS;
        }
    } while(group != startgroup);

    CoreGetResponse *res = (CoreGetResponse*) dispatchAllocate(chan, sizeof(CoreGetResponse));

    res->core = core;
    res->ack = ack;

    dispatchRespond(chan, trans, res, sizeof(CoreGetResponse));
}

void coreserver_idleloop(void *)
{
    std::set<pid_t>::iterator it = g_pollpids.begin();
    while(it != g_pollpids.end())
    {
        char path[1024];
        snprintf(path, 1024, "/proc/%d/status", *it);
        FILE *fd = fopen(path, "r");

        if(fd != NULL)
            fclose(fd);

        if(fd == NULL)
        {
            // Process died, reclaim cores
            printf("Reclaiming cores for pid %u\n", *it);
            CoreList &cores = g_pidcore.find(*it)->second;
            for(CoreList::reverse_iterator it2 = cores.rbegin(); it2 != cores.rend(); ++it2)
            {
                g_coregroup[(*it2) % CONFIG_CORESERV_GROUPS].push_front(*it2);
            }

            g_pidcore.erase(*it);
#if __GNUC__ == 4 && __GUNC_MINOR__ > 3
            it = g_pollpids.erase(it);
#else
            g_pollpids.erase(it);
#endif
            continue;
        }

        ++it;
    }

}
