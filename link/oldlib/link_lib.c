#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <arpa/inet.h>

#include <env/env.h>
#include <rpc/rpc.h>
#include <sys/net/link.h>
#include <sys/net/types.h>
#include <uk_syscall/syscall.h>
#include "./link_private.h"
#include <config/config.h>

//#define USE_SYSCALL_DIRECTLY 1

static FosRemotebox net_link;
static FosRemotebox net_eth;
static bool net_link_init = false;

FosRemotebox g_master_netstack;

static inline void link_init()
{
    if (!net_link_init)
    {
        net_link_init = true;
        fosEnvLookupMailboxCapabilityStr(&net_link.alias, &net_link.capability, fosEnv(), "/sys/net/@local/link");
        fosEnvLookupMailboxCapabilityStr(&net_eth.alias, &net_eth.capability, fosEnv(), "/sys/net/@local/if/eth0_out");
    }
}

FosStatus netLinkSend(
    struct in_addr * in_to,
    const void * in_data,
    size_t in_size)
{

#if defined(USE_SYSCALL_DIRECTLY)
    __do_fos_syscall_typed(SYSCALL_NETIF_SEND, in_data, in_size, 0, 0, 0, 0);
    return FOS_MAILBOX_STATUS_OK;
#else
    link_init();

    LinkSendMessage msg;
    msg.size = in_size;

    struct iovec iov[2];
    iov[0].iov_base = &msg;
    iov[0].iov_len = sizeof(msg);
    iov[1].iov_base = (void *)in_data;
    iov[1].iov_len = in_size;

    dispatchSendRequestV(&net_eth.alias, net_eth.capability, iov, 2, NET_LINK_SEND);

    return FOS_MAILBOX_STATUS_OK;
#endif
}

FosStatus netLinkSetLoMaster(const FosRemotebox * in_master)
{
    g_master_netstack = *in_master;

    return FOS_MAILBOX_STATUS_OK;
}

FosStatus netLinkSetMaster(const FosRemotebox * master)
{
    link_init();

    LinkSetMasterMessage * msg = (LinkSetMasterMessage *) malloc(sizeof(LinkSetMasterMessage));

    msg->reply = *rpcGetResponseMailbox();
    msg->master = *master;

    DispatchToken token;
    token = dispatchSendRequest(&net_link.alias, net_link.capability, msg, sizeof(*msg), NET_LINK_SET_MASTER);

    free(msg);

    DispatchResponse * response = rpcWaitForResponse(token);
    FosStatus ret = ((LinkSetMasterResponse*)response->data)->status;
    dispatchResponseFree(response);

    return ret;
}

FosStatus netLinkIncomingV(struct iovec * in_iov, size_t in_iovcnt)
{
    link_init();
#if ENABLE_SKIP_LINK_ON_LOOPBACK
    unsigned int i;
    unsigned int size = 0;
    uint8_t *data;
    if(in_iovcnt > 1) // if we're split up into sub buffers, merge into one data chunk
    {

        // first determine the size
        for(i = 0; i < in_iovcnt; i++)
            size += in_iov[i].iov_len;

        // now create memory and copy into it
        data = (uint8_t *)malloc(size);
        unsigned int index = 0;
        for(i = 0; i < in_iovcnt; i++)
        {
            memcpy(data + index, in_iov[i].iov_base, in_iov[i].iov_len);
            index += in_iov[i].iov_len;
        }
    }
    else // if it's just one buffer then we don't need to allocate / copy
    {
        data = in_iov[0].iov_base;
        size = in_iov[0].iov_len;
    }

    // now process the data as if we were in the link server as
    // we'll just be sending back out to a transport from here
    link_handle_incoming(data, size, &g_master_netstack);

    if(in_iovcnt > 1) free(data); // we had to allocate data, now free it

    return FOS_MAILBOX_STATUS_OK;

#else

    LinkIncomingMessage msg;

    msg.reply = *rpcGetResponseMailbox();

    struct iovec iov[in_iovcnt + 1];
    iov[0].iov_base = &msg;
    iov[0].iov_len = sizeof(msg);

    unsigned int i;
    unsigned int size = 0;
    for(i = 0; i < in_iovcnt; i++)
    {
        iov[i+1].iov_base = in_iov[i].iov_base;
        iov[i+1].iov_len = in_iov[i].iov_len;
        size += in_iov[i].iov_len;
    }

    msg.size = size;

    DispatchToken token;
    token = dispatchSendRequestV(&net_link.alias, net_link.capability, iov, in_iovcnt + 1, NET_LINK_INCOMING);

//    free(msg);
/*
    DispatchResponse * response = rpcWaitForResponse(token);
    FosStatus ret = ((LinkIncomingResponse*)response->data)->status;
    dispatchResponseFree(response);
*/
    FosStatus ret = FOS_MAILBOX_STATUS_OK;
    return ret;
#endif
}

FosStatus netLinkIncoming(const void * in_data, const size_t in_size)
{
    struct iovec iov[1];
    iov[0].iov_base = (void *)in_data;
    iov[0].iov_len = in_size;

    return netLinkIncomingV(iov, 1);
}

FosStatus netLinkGetMac(MacAddress * mac)
{
    link_init();

    LinkGetMacMessage * msg = (LinkGetMacMessage *) malloc(sizeof(LinkGetMacMessage));

    msg->reply = *rpcGetResponseMailbox();

    DispatchToken token;
    token = dispatchSendRequest(&net_link.alias, net_link.capability, msg, sizeof(*msg), NET_LINK_GET_MAC_ADDRESS);

    free(msg);

    DispatchResponse * dispatch_response = rpcWaitForResponse(token);
    LinkGetMacResponse * response = (LinkGetMacResponse*)dispatch_response->data;

    FosStatus ret = response->status;
    if (ret == FOS_MAILBOX_STATUS_OK)
        *mac = response->mac;

    dispatchResponseFree(dispatch_response);

    return ret;
}

FosStatus netLinkShutdown()
{
    link_init();

    LinkShutdownMessage * msg = (LinkShutdownMessage *) malloc(sizeof(LinkShutdownMessage));

    msg->reply = *rpcGetResponseMailbox();

    DispatchToken token;
    token = dispatchSendRequest(&net_link.alias, net_link.capability, msg, sizeof(*msg), NET_LINK_SHUTDOWN);

    free(msg);

    DispatchResponse * dispatch_response = rpcWaitForResponse(token);
    LinkShutdownResponse * response = (LinkShutdownResponse*)dispatch_response->data;

    FosStatus ret = response->status;

    dispatchResponseFree(dispatch_response);

    return ret;
}
