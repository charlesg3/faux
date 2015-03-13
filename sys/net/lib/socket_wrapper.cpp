#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <assert.h>
#include <regex.h>

#include <utilities/debug.h>
#include <rpc/rpc.h>
#include <system/status.hpp>
#include "socket.hpp"

using namespace fos;
using namespace fos::net::app;

ssize_t fosTransportWrite(struct FosFDEntry *file, const void * in_buf, size_t in_count)
{
    FlowSocket *sock = (FlowSocket *)file->private_data;
    return sock->write(in_buf, in_count);
}

int fosTransportClose(struct FosFDEntry *file)
{
    int ret;
    Socket *sock = (Socket *)file->private_data;
    sock->close();
    delete sock;
    return ret;
}

ssize_t fosTransportRead(struct FosFDEntry *file, void *buf, size_t nbytes)
{
    FlowSocket *sock = (FlowSocket *)file->private_data;
    return sock->read(buf, nbytes, !(file->status_flags & O_NONBLOCK));
}

int fosTransportPoll(struct FosFDEntry *file)
{
    Socket *sock = (Socket *)file->private_data;
    return sock->poll();
}

int fosTransportGetPeer(struct FosFDEntry *file, struct sockaddr * out_address)
{
    Socket *sock = (Socket *)file->private_data;
    return sock->getPeer(out_address);
}

int fosTransportConnect(struct FosFDEntry *file, const struct sockaddr * in_address, socklen_t in_address_len)
{
    // Here we have to ditch the old sock because it was created as a listen socket (in socket.c)
    // and we have to create a new FlowSocket in it's place, and then call the connect() function
    // on the flow socket

    // First extract relevant info out of old socket
    Socket *sock = (Socket *)file->private_data;
    int domain = sock->getDomain();
    int type = sock->getPType();
    int protocol = sock->getProtocol();
    delete sock;

    // Now create the new socket
    int rc = fosTransportInitFlowSocket(&file->private_data, domain, type, protocol);

    // Now call the connect() function on the new (converted) sock
    FlowSocket *flow_sock = (FlowSocket *)file->private_data;
    return flow_sock->connect(in_address, in_address_len);
}

int fosTransportBind(struct FosFDEntry *file, const struct sockaddr * in_address, socklen_t in_address_len)
{
    ListenSocket *sock = (ListenSocket *)file->private_data;
    return sock->bind(in_address, in_address_len);
}

int fosTransportListen(struct FosFDEntry *file, int in_backlog)
{
    ListenSocket *sock = (ListenSocket *)file->private_data;
    return sock->listen(in_backlog);
}

int fosTransportAccept(struct FosFDEntry *file, void **vp, struct sockaddr * in_address, socklen_t * in_address_len, int flags)
{
    ListenSocket *sock = (ListenSocket *)file->private_data;
    bool nonblocking = file->status_flags & O_NONBLOCK;
    nonblocking |= flags & SOCK_NONBLOCK;
    return sock->accept(vp, in_address, in_address_len, !nonblocking);
}

int fosTransportStat(const struct FosFDEntry *file, struct stat *buf)
{
    Socket *sock = (Socket *)file->private_data;
    return sock->stat();
}


Status fosTransportGetHostByName(struct in_addr * out_addr, const char * in_name)
{
#if 0
    // begin rpc boilerplate code
    size_t len; 
    if (in_name == NULL) 
        len =0;
    else
        len= strlen(in_name) + 1;

    GetHostByNameRequest * req = GetHostByNameRequest::allocate(len);
    req->rbox = *rpcGetResponseMailbox();
    req->len = len;
    strncpy(&req->hostname, in_name, len);

    Remotebox netstack;
    netstack.ref().alias = Alias("/sys/net/dns_lookup").ref(); //FIXME: why is this needed?

    if(fosEnvLookupMailboxCapability(&netstack.capability(), fosEnv(), &netstack.ref().alias) != FOS_MAILBOX_STATUS_OK)
    {
        netstack.ref().alias = Alias("/sys/net/@local/dns_lookup").ref();
        if(fosEnvLookupMailboxCapability(&netstack.capability(), fosEnv(), &netstack.ref().alias) != FOS_MAILBOX_STATUS_OK)
        {
            PS("Alias lookup failure in dns query.");
            return FOS_MAILBOX_STATUS_INVALID_ALIAS_ERROR;
        }
    }

    DispatchToken token = dispatchSendRequest(&netstack.alias(), netstack.capability(),
                                              req, req->size(), GET_HOST_BY_NAME);

    delete req;

    DispatchResponse * rc = rpcWaitForResponse(token);

    GetHostByNameReply * reply = (GetHostByNameReply *)rc->data;

    FosStatus ret = reply->status;
    if (ret == FOS_MAILBOX_STATUS_OK)
        *out_addr = reply->addr;

    dispatchResponseFree(rc);
#endif
    return FOS_MAILBOX_STATUS_OK;
}

FosStatus fosTransportInitListenSocket(void **vp, int domain, int type, int protocol)
{
    *vp = new ListenSocket(domain, type, protocol);
    return *vp ? FOS_MAILBOX_STATUS_OK : FOS_MAILBOX_STATUS_ALLOCATION_ERROR;
}

FosStatus fosTransportInitFlowSocket(void **vp, int domain, int type, int protocol)
{
    *vp = new FlowSocket(domain, type, protocol);
    return *vp ? FOS_MAILBOX_STATUS_OK : FOS_MAILBOX_STATUS_ALLOCATION_ERROR;
}

FosStatus fosTransportSetShared(struct FosFDEntry *file, bool shared)
{
    ListenSocket *sock = (ListenSocket *)file->private_data;
    return sock->setShared(shared);
}

FosStatus fosTransportRecvFrom(struct FosFDEntry *file, void *mem, size_t len, int flags,
      struct sockaddr * __restrict from, socklen_t * __restrict fromlen)
{
    Socket *sock = (Socket *)file->private_data;
    return sock->recvFrom(mem, len, flags, from, fromlen);
}

FosStatus fosTransportSendTo(struct FosFDEntry *file, const void *dataptr, size_t size, int flags,
    const struct sockaddr *to, socklen_t tolen)
{
    Socket *sock = (Socket *)file->private_data;
    return sock->sendTo(dataptr, size, flags, to, tolen);
}

void fosTransportDumpStats()
{
    Socket::dumpStats();
}

FosFileOperations fosSocketOps =
{
    fosTransportRead,
    fosTransportWrite,
    fosTransportClose,
    fosTransportPoll,
    NULL,
    fosTransportStat,
    NULL,
};
#if 0
// Packet tracing function
bool trace_check(const char *data)
{
    // Comple the regex if we haven't already
    int reti;
    char msgbuf[100];
    static bool initted = false;
    static regex_t trace_regex;
    //char *check = "HTTP/";
    char *check= "^GET";

    if(!initted)
    {
        reti = regcomp(&trace_regex, check, 0);
        if( reti ){ fprintf(stderr, "Could not compile regex\n"); exit(1); }
        initted = true;
    }

    // Check the regex
    reti = regexec(&trace_regex, data, 0, NULL, 0);
    if( !reti || reti == REG_NOMATCH)
        return !reti;
    else
    {
        regerror(reti, &trace_regex, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Regex match failed: %s\n", msgbuf);
        exit(1);
        return 0;
    }
}
#endif
