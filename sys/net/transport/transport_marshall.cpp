#include <messaging/dispatch.h>

#include "transport_marshall.hpp"
#include "transport_server.hpp"
//#include "sys/net/init.h"
namespace fos { namespace net {
namespace transport {

#if 0
    void idle(void * vptrans)
    {
        ((Transport *)vptrans)->idle();
    }

    void incoming(void * data, size_t len, void * vptrans)
    {
        ((Transport *)vptrans)->incoming(data, len);
    }

    void transferData(void * data, size_t len, void * vptrans)
    {
        ((Transport *)vptrans)->transferData(data, len);
    }
#endif

    void write(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
    {
        assert(vpmsg != NULL);

        WriteRequest * msg = (WriteRequest *)(vpmsg);
        WriteReply * reply = (WriteReply *) dispatchAllocate(chan, sizeof(WriteReply));

        reply->ret = Transport::transport().write(msg->flow_id, msg->data, msg->len);

        dispatchFree(trans, vpmsg);
        dispatchRespond(chan, trans, reply, sizeof(WriteReply));
    }

    void close(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
    {
        assert(vpmsg != NULL);

        CloseRequest * msg = (CloseRequest *)(vpmsg);
        CloseReply * reply = (CloseReply *) dispatchAllocate(chan, sizeof(CloseReply));

        reply->status = Transport::transport().close(msg->flow_id);

        dispatchFree(trans, vpmsg);

        dispatchRespond(chan, trans, reply, sizeof(CloseReply));
    }

    void setIP(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
    {
        assert(vpmsg != NULL);

        SetIPRequest * msg = (SetIPRequest *)(vpmsg);
        SetIPReply * reply = (SetIPReply *) dispatchAllocate(chan, sizeof(SetIPReply));

        reply->status = Transport::transport().setIP(msg->ip_address, msg->netmask, msg->gw);

        dispatchFree(trans, vpmsg);

        dispatchRespond(chan, trans, reply, sizeof(SetIPReply));
    }

    void incoming(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
    {
        assert(vpmsg != NULL);

        IncomingRequest * msg = (IncomingRequest *)(vpmsg);
        IncomingReply * reply = (IncomingReply *) dispatchAllocate(chan, sizeof(IncomingReply));

        Transport::transport().incoming(msg->data, msg->len);
        reply->status = FOS_MAILBOX_STATUS_OK;

        dispatchFree(trans, vpmsg);

        dispatchRespond(chan, trans, reply, sizeof(IncomingReply));
    }

    void provideAppWithConnectionCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
    {
        // Unwrap parameters
        conn::ConnectionAcceptedEventTransport * req = (conn::ConnectionAcceptedEventTransport *) msg;

        conn::ConnectionAcceptedEventTransportReply *reply = 
            (conn::ConnectionAcceptedEventTransportReply *) dispatchAllocate(chan, sizeof(*reply));
        Transport::transport().provideAppWithConnection(req->flow_id, req->app, 
                &reply->is_closed, &reply->already_claimed, &reply->oops_valid_app);

        dispatchFree(trans, msg);
        dispatchRespond(chan, trans, reply, sizeof(*reply));
    }

#if 0

    void sendTo(void * vpmsg, size_t size, DispatchToken token, void * vptrans)
    {
        uint8_t *pmsg = (uint8_t *)vpmsg;
        SendToRequest * msg = (SendToRequest *) (pmsg + sizeof(DispatchMessageHeader));
        void *data = (void *)(pmsg + sizeof(DispatchMessageHeader) + sizeof(SendToRequest));

        SendToReply reply;
        reply.status = ((Transport *)vptrans)->sendTo(msg->connection_id, data, msg->_size, msg->addr, msg->port);

//        dispatchSendResponse(&msg->rbox.alias(), msg->rbox.capability(), &reply, sizeof(reply), token);
    }

    void recvFrom(void * vpmsg, size_t size, DispatchToken token, void * vptrans)
    {
        WriteRequest * msg = (WriteRequest *) vpmsg;
        WriteReply reply;
        reply.ret = ((Transport *)vptrans)->write(msg->connection_id, &msg->data, msg->len);
        dispatchSendResponse(&msg->rbox.alias(), msg->rbox.capability(), &reply, sizeof(reply), token);
    }

    void getPeer(void * vpmsg, size_t size, DispatchToken token, void * vptrans)
    {
        GetPeerRequest * msg = (GetPeerRequest *) vpmsg;
        GetPeerReply reply;
        reply.status = ((Transport *)vptrans)->getPeer(msg->connection_id, (in_addr_t *)&reply.addr);
        dispatchSendResponse(&msg->rbox.alias(), msg->rbox.capability(), &reply, sizeof(reply), token);
    }

#endif

#if 0

    struct GetHostByNameData
    {
        messaging::Remotebox rbox;
        DispatchToken token;
    };

    void hostFound(const char * hostname, struct ip_addr * ipaddr, void * vparg)
    {
        GetHostByNameData * data = (GetHostByNameData *) vparg;
        in_addr_t * addr = (in_addr_t *)ipaddr;

        GetHostByNameReply reply;

        if (ipaddr)
        {
            reply.addr = * (in_addr *) addr;
            reply.status = FOS_MAILBOX_STATUS_OK;
        }
        else
        {
            memset(&reply.addr, 0, sizeof(reply.addr));
            reply.status = FOS_MAILBOX_STATUS_GENERAL_ERROR;
        }

        dispatchSendResponse(&data->rbox.alias(), data->rbox.capability(), &reply, sizeof(reply), data->token);
        delete data;
    }

    void getHostByName(void * vpmsg, size_t size, DispatchToken token, void * vptrans)
    {
        GetHostByNameRequest * msg = (GetHostByNameRequest *) vpmsg;
        GetHostByNameReply reply;
        GetHostByNameData * data = new GetHostByNameData();
        data->rbox = msg->rbox;
        data->token = token;
        reply.status = ((Transport *)vptrans)->getHostByName(msg->rbox, &msg->hostname, &reply.addr, data);

        // if we get RESOURCE_UNAVAILABLE, the hostFound callback
        // above will take care of deleting and returning the response
        if (reply.status != FOS_MAILBOX_STATUS_RESOURCE_UNAVAILABLE)
        {
            delete data;
            dispatchSendResponse(&msg->rbox.alias(), msg->rbox.capability(), &reply, sizeof(reply), token);
        }
    }

    void netShutdown(void * vpmsg, size_t size, DispatchToken token, void * vptrans)
    {
        struct NetShutdownRequest * msg = (struct NetShutdownRequest*) vpmsg;
        struct NetShutdownReply reply;
        ((Transport *)vptrans)->netShutdown();
        reply.status = FOS_MAILBOX_STATUS_OK;
        dispatchSendResponse(&msg->rbox.alias, msg->rbox.capability, &reply, sizeof(reply), token);
    }
#endif
}}}
