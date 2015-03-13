#include <assert.h>
#include "conn_manager.hpp"
#include "conn_private.hpp"
#include "conn_marshall.hpp"
#include "transport_server.hpp"

namespace fos { namespace net { namespace conn {

    /* ConnServer callbacks */

    void doBindAndListenCallback(Channel *chan, DispatchTransaction trans, void *msg, size_t size, bool notify_peers)
    {
        assert(msg != NULL);
        assert(size == sizeof(BindAndListenRequest));

        BindAndListenRequest* req = (BindAndListenRequest*) msg;
        BindAndListenReply* reply = (BindAndListenReply*) dispatchAllocate(chan, sizeof(BindAndListenReply));
        ConnManager &conn = ConnManager::connManager();

        reply->status = conn.bindAndListen(req->addr, req->port, req->shared, notify_peers, &reply->listen_sock_id);

        dispatchFree(trans, msg);
        dispatchRespond(chan, trans, reply, sizeof(*reply));
    }

    void bindAndListenCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
    {
        doBindAndListenCallback(chan, trans, msg, size, true);
    }

    void peerBindAndListenCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
    {
        doBindAndListenCallback(chan, trans, msg, size, false);
    }

    void acceptCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
    {
        struct SetAcceptingRequest *req = (struct SetAcceptingRequest *) msg;
        SetAcceptingReply *reply = (SetAcceptingReply *) dispatchAllocate(chan, sizeof(SetAcceptingReply));;
        ConnManager &conn = ConnManager::connManager();

        reply->status = conn.accept(req->listen_id, req->app, &reply->connection_available);

        dispatchFree(trans, msg);
        dispatchRespond(chan, trans, reply, sizeof(*reply));
    }

    void incomingConn(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
    {
        ConnManager &conn = ConnManager::connManager();

        SynRequest *req = (SynRequest *) msg;

        SynReply *reply = (SynReply *) dispatchAllocate(chan, sizeof(*reply));

        reply->transport_id = conn.incomingConn(req->source, req->source_port,
                                            req->dest, req->dest_port,
                                            req->syn, req->syn_len);

        dispatchFree(trans, msg);
        dispatchRespond(chan, trans, reply, sizeof(*reply));
    }

    /* ConnClient callbacks */

    void establishIncomingConnCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
    {
        ConnTransport &conn = ConnTransport::connTransport();

        ConnectionAcceptedEventTransportSyn *req = (ConnectionAcceptedEventTransportSyn *) msg;

        conn.establishIncomingConn(chan, req->listen_sock_id, req->flow_id,
                req->addr, req->port,
                req->acceptor, req->acceptor_valid,
                req->syn, req->syn_len);

        dispatchFree(trans, msg);
    }

}}}
