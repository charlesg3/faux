#include <cassert>
#include <cstdio>

#include <messaging/dispatch.h>
#include <sys/server/server.h>
#include <sys/sched/interface.h>

#include "../fs/server.hpp"

#include "marshall.hpp"
#include "server.hpp"

namespace fos { namespace sched {

struct execArgs
{
    Channel *channel;
    DispatchTransaction trans;
    ExecReply *reply;
};

void exec_callback(void *p, int type, int rc, int errno_reply)
{
    struct execArgs *ea = (struct execArgs *)p;
    ea->reply->type = type;
    ea->reply->retval = rc;
    ea->reply->errno_reply = errno_reply;
    dispatchRespond(ea->channel, ea->trans, ea->reply, sizeof(ExecReply));
    free(ea);
}

void exec(Channel* channel, DispatchTransaction trans, void* vpmsg, size_t size_)
{
    assert(vpmsg);

    SchedServer & ss = SchedServer::ss();

    // execing app closes.
    if(ss.id < CONFIG_FS_SERVER_COUNT)
    {
        fs::FilesystemServer & fs = fs::FilesystemServer::fs();
        fs.appClose((uintptr_t)channel);
    }

    ExecRequest* request = static_cast<ExecRequest*>(vpmsg);
//    StatusReply* reply = static_cast<StatusReply*>(dispatchAllocate(channel, sizeof(*reply)));
//    ExecReply* reply = static_cast<ExecReply*>(dispatchAllocate(channel, sizeof(*reply)));
//    reply->retval = OK;

    char *args = request->filename + strlen(request->filename) + 1;
    char *envs = request->filename + request->envp_offset;

    /*
    struct execArgs *ea = (struct execArgs *)malloc(sizeof(struct execArgs));
    ea->channel = channel;
    ea->trans = trans;
    ea->reply = reply;
    */

    //ss.exec(request->pid, request->filename, args, envs, exec_callback, ea);
    ss.exec(request->pid, request->filename, args, envs, exec_callback, NULL);
    dispatchFree(trans, vpmsg);
}

}}
