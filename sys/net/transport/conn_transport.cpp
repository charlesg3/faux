#include <assert.h>

#include "transport_private.hpp"
#include "transport_server.hpp"
#include "conn_private.hpp"

#include <utilities/tsc.h>
#include <utilities/console_colors.h>

#ifdef CONFIG_PROF_NETSTACK
extern uint64_t g_tcp_sends;
extern uint64_t g_tcp_bytes_sent;
extern uint64_t g_tcp_cycles_spent;
extern uint64_t g_tcp_fails;
extern uint64_t g_link_send_cycles;
extern uint64_t g_connects;
extern uint64_t g_accepts;
extern uint64_t g_claims;
#endif

namespace fos { namespace net { namespace transport {

#if 0

// The three-way handshakes follow an unusual control flow, and
// therefore are not written in the typical RPC-function-oriented
// style, instead the messaging is put in with the control flow where
// convenient.

// Called by connection manager to transfer a connection to an
// application -- does TCP processing and then calls
// provideAppWithConnection
void conn::transferConnection(void * vpmsg, size_t size, DispatchToken token, void * vptrans)
{
    USE_DEBUG_NETSTACK(unsigned long long time = rdtscll();)
    Status rc;

    // Unwrap message
    Transport * transport = (Transport *) vptrans;
    ConnectionAcceptedEventTransportWithPcb * msg = (ConnectionAcceptedEventTransportWithPcb *) vpmsg;
    assert(size == sizeof(ConnectionAcceptedEventTransportWithPcb));

    NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
            "start transfer connection request for lp: %d rp: %d time: %lld." CONSOLE_COLOR_NORMAL,
            msg->pcb.local_port, msg->pcb.remote_port, time);

    // this does the TCP part of setting up a connection in the Transport class
    // i.e. it sets up the pcb and adds it to the active pcb list
    rc = transport->establishConnection(&msg->pcb, msg->app_data.connection_id);

    assert(rc == FOS_MAILBOX_STATUS_OK);

    // If we have a valid acceptor, then go ahead and try to start the
    // handshake, otherwise we should ACK the connection manager (this
    // ACK is generated in provideAppWithConnection on the other path)
    if (msg->acceptor_valid)
    {
        NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                "  transfer connection request for lp: %d rp: %d: " CONSOLE_COLOR_GREEN "VALID" CONSOLE_COLOR_NORMAL " calling provideAppWithConnection "
                " with flow: " CONSOLE_COLOR_YELLOW "[%x]" CONSOLE_COLOR_NORMAL,
                msg->pcb.local_port, msg->pcb.remote_port, msg->app_data.connection_id);
        //printf("calling provideAppWithConnection\n");
        //ignore end of message that has the PCB in it
        provideAppWithConnection(vpmsg, sizeof(ConnectionAcceptedEventTransport), token, vptrans);
    }
    else
    {
        NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                "  transfer connection request for lp: %d rp: %d: " CONSOLE_COLOR_RED "INVALID" CONSOLE_COLOR_NORMAL ,
                msg->pcb.local_port, msg->pcb.remote_port);
        ConnectionAcceptedEventTransportReply reply;
        reply.already_claimed = false;
        dispatchSendResponse(&msg->rbox.alias(), msg->rbox.capability(), &reply, sizeof(reply), token);
    }
    NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
            "done transfer connection request for lp: %d rp: %d time: %lld." CONSOLE_COLOR_NORMAL,
            msg->pcb.local_port, msg->pcb.remote_port, time);
}

// Invoked by connection manager (and internally) to initiate a
// three-way handshake with applications to match an app with a
// connection.
// 
// This thread sits in a loop messaging applications, sleeping, and
// checking if the message has been claimed. If not, it gets a new
// acceptor from the connection manager and repeats (unless no
// acceptor is available).
//
// It is possible to enter this handler after an application that we
// assumed had timed out wakes and claims the connection, in which
// case we terminate.
// 
// Note: a connection will only be in this loop iff it is NOT on the
// unclaimed connection list in the connection manager.
//
//  - This handler is only entered when a connection starts and an app
//    was available to claim the connection, in which case it is not
//    on the list; or when it is removed from the list.
//
//  - This handler terminates if no applications are available to
//    claim the connection.

void conn::provideAppWithConnection(void *vpmsg, size_t size, DispatchToken cm_token, void *vptrans)
{
    // Unwrap parameters
    Transport * transport = (Transport *) vptrans;
    ConnectionAcceptedEventTransport * msg = (ConnectionAcceptedEventTransport *) vpmsg;
    //fprintf(stderr, "in provideAppWithConnection size=%u should be %u ~withPcb = %u\n", size, sizeof(ConnectionAcceptedEventTransportWithPcb), sizeof(ConnectionAcceptedEventTransport));
    assert(size == sizeof(ConnectionAcceptedEventTransport));
    assert(msg->acceptor_valid);

    NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
            "Sending connection to acceptor: " CONSOLE_COLOR_YELLOW "[%s]" CONSOLE_COLOR_NORMAL
            " For flow: " CONSOLE_COLOR_YELLOW "[%x]" CONSOLE_COLOR_NORMAL, &msg->acceptor.alias(),
            msg->app_data.connection_id);

    // check if the flow has already been claimed, send response to conn mgr
    const flow_t flow_id(msg->app_data.connection_id);
    FlowSocket * flow;

    transport->isClaimed(flow_id, &flow);
    if (!flow)
    {
        // bad socket type? ignore request
        NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                "No associated flow to connection id : %x", flow_id);
        ConnectionAcceptedEventTransportReply reply;
        reply.already_claimed = true;
        dispatchSendResponse(&msg->rbox.alias(), msg->rbox.capability(), &reply, sizeof(reply), cm_token);
        return;
    }

    NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
        "new connection: %x" CONSOLE_COLOR_NORMAL, flow_id);

    ConnectionAcceptedEventTransportReply reply;
    reply.already_claimed = flow->m_claimed;
    if (flow->m_claimed)
    {
        reply.oops_valid_app = flow->m_client_addr;
    }

    dispatchSendResponse(&msg->rbox.alias(), msg->rbox.capability(), &reply, sizeof(reply), cm_token);

    Remotebox acceptor = msg->acceptor;

    DispatchToken our_token = dispatchCreateToken();
    msg->app_data.token = our_token;

#if CONFIG_HANDLE_APP_TIMEOUT
    // Loop until either: an app has claimed this connection OR no app is available to claim it
    while (true)
    {
        // Message application to initiate handshake
        NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                "sending data to:" CONSOLE_COLOR_YELLOW "[%s]" CONSOLE_COLOR_NORMAL,
                &acceptor.alias());
        acceptor.send(msg->app_data);
        NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                "sent data to:" CONSOLE_COLOR_YELLOW "[%s]" CONSOLE_COLOR_NORMAL,
                &acceptor.alias());

        // If an application replies to the handshake the message is
        // handled outside of this function and metadata is stored to
        // record this. Applications will respond to the above
        // message by sending a request, which enters
        // claimConnection() below.
        //
        // the connection may be closed by the time this timeout ends
        // however we still recognize the flow as "claimed" and return
        dispatchSleep(TRANSPORT_TIMEOUT);

        FlowSocket *out_flow;
        bool is_claimed = transport->isClaimed(flow_id, &out_flow);

        if (out_flow && is_claimed)
        {
            NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                    " new connection " CONSOLE_COLOR_YELLOW "[%x]" CONSOLE_COLOR_NORMAL " claimed." CONSOLE_COLOR_NORMAL,
                    flow_id);
            // Success - we are done
            return;
        }
        else if(out_flow) // flow not claimed
        {
            // timeout, tell CM that this app didn't respond and get a
            // new app
            NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                    " %d timeout on listening socket id: %x, notifying ConnectionManager", 
                    transport->index(), msg->listening_socket_id);
            TransportApplicationTimeout timeout;
            timeout.connection = (Connection) msg->app_data;
            timeout.listening_socket_id = msg->listening_socket_id;
            timeout.timed_out_app = acceptor;

            // make sure this goes to the listening netstack
            DispatchToken timeout_token = dispatchSendRequest(&transport->cmRemoteBox().alias(), transport->cmRemoteBox().capability(), 
                    &timeout, sizeof(timeout), TRANSPORT_APPLICATION_TIMEOUT);

            NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                    " %d waiting for response from connection manager.",
                    transport->index());

            DispatchResponse * response = dispatchWaitForToken(timeout_token);
            assert(response);

            NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                    " Got response.");

            TransportApplicationTimeoutReply * reply;
            reply = (TransportApplicationTimeoutReply *)response->data;

            if (!reply->is_valid)
            {
                // No new acceptor available, we are now on the
                // pending connection list and this loop should
                // terminate. A new one will be started when an
                // application requests a connection, and its possible
                // that other applications can claim this connection
                // by calling into claimConnection below.
                NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                        " reply invalid, leaving.");
                dispatchResponseFree(response);
                return;
            }
            else
                NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                        " reply is valid.");

            acceptor = reply->acceptor;
            dispatchResponseFree(response);
        }
        else // out_flow must be gone because app closed it -- assume it was claimed --cg3
        {
            return;
        }

    }
    NET_DEBUG(CONSOLE_COLOR_YELLOW " SHOULDN'T GET HERE!!! Transport: new connection claimed." CONSOLE_COLOR_NORMAL);
#else
    acceptor.send(msg->app_data);
#endif
}

#endif

// Invoked by application to attempt to claim a connection that it was
// sent a ConnectionAcceptEvent for previously. Will only succeed if
// the connection is unclaimed.
void claimConnection(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    // Unwrap parameters
    Transport & transport = Transport::transport();
    FlowSocket * flow;
    conn::ConnectionAcceptedReply * msg = (conn::ConnectionAcceptedReply *) vpmsg;
    assert(size == sizeof(conn::ConnectionAcceptedReply));

    // Check if this connection is available
    bool available = !transport.isClaimed(msg->flow_id, &flow);

    if (!flow)
    {
        // bad socket type? ignore request
        PS("No associated flow to connection id : %x", msg->flow_id);
        // Tell app whether or not it got it...
        conn::ConnectionAcceptedAck *ack = (conn::ConnectionAcceptedAck *) channel_allocate(chan, sizeof(conn::ConnectionAcceptedAck));
        ack->status = FOS_MAILBOX_STATUS_RESOURCE_UNAVAILABLE;
        dispatchRespond(chan, trans, ack, sizeof(conn::ConnectionAcceptedAck));
        dispatchFree(trans, vpmsg);
        return;
    }

    dispatchFree(trans, vpmsg);

    // Tell app whether or not it got it...
    conn::ConnectionAcceptedAck *ack = (conn::ConnectionAcceptedAck *) dispatchAllocate(chan, sizeof(conn::ConnectionAcceptedAck));
    assert(ack);
    ack->status = available ? FOS_MAILBOX_STATUS_OK : FOS_MAILBOX_STATUS_RESOURCE_UNAVAILABLE;
    dispatchRespond(chan, trans, ack, sizeof(conn::ConnectionAcceptedAck));

    if (available)
    {
        // Claim the connection for this application
        USE_PROF_NETSTACK(g_claims++);
        flow->m_claimed = true;

        //Establish a new channel with the client
        Endpoint *ep = Transport::transport().getEndpoint();
        flow->m_client.async = endpoint_connect(ep, msg->conn_sock_addr);
        flow->m_client.sync = endpoint_connect(ep, msg->conn_sock_addr);
        dispatchListenChannel(flow->m_client.sync);
    }

    transport.pushRefusedPackets(flow->m_flow_id);
}

void connect(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    Transport & transport = Transport::transport();
    transport::ConnectRequest * msg = (transport::ConnectRequest *) vpmsg;
    transport.connect(chan, trans, msg->client_addr, msg->addr, msg->port);
    dispatchFree(trans, vpmsg);
}

}}}
