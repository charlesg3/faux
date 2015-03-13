#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <name/name.h>
#include <lwip/inet.h>
#include <sys/net/transport.h>
#include <sys/net/types.h>
#include "conn_marshall.hpp"
#include "conn_manager.hpp"
#include "transport_server.hpp"
#include "../common/include/hash.h"
#include "../net.hpp"

#define MBOX_SIZE (64 * 1024)
namespace fos { namespace net { namespace conn {

    /* Static declarations */

    ConnLink *ConnLink::g_conn_link;
    ConnTransport *ConnTransport::g_conn_transport;
    ConnManager *ConnManager::g_conn_manager;

    /* Public methods */

    ConnManagerTransportLink::ConnManagerTransportLink(transport::Transport *trans, 
            int id,
            int num_transports)
        : m_transport(trans)
        , m_num_transports(num_transports)
        , m_id(id)
    {
        m_endpoint = endpoint_create();
        m_addr = endpoint_get_address(m_endpoint);

        dispatchRegister(ESTABLISH_INCOMING_CONN, establishIncomingConnCallback);

        dispatchRegister(BIND_AND_LISTEN, bindAndListenCallback);
        dispatchRegister(PEER_BIND_AND_LISTEN, peerBindAndListenCallback);
        dispatchRegister(ACCEPT, acceptCallback);

        assert(!(g_conn_transport) && !(g_conn_manager));

        g_conn_link = this;
        g_conn_transport = this;
        g_conn_manager = this;

        m_curr_transport = m_id;

        // Register under the common name

        // Address addr = m_transport->getAddr();
        Name name = CONN_MANAGER_NAME;
        name_register(name, m_addr);

        // Register under the specific name (used by peers)
        name = CONN_MANAGER_NAME + m_id + 1;
        name_register(name, m_addr);

        // m_peers.reserve(m_num_transports);

        for(int i = 0; i < m_num_transports; i++)
            m_peers.push_back(Peer(-1, NULL));

        for(int i = 0; i < m_id; i++)
        {
            // Accept incoming endpoint_connect() request from all CMs with id's
            // lower than us
            while(!name_lookup(CONN_MANAGER_NAME + i + 1, &m_peers[i].addr))
                ;

            while(!(m_peers[i].chan = endpoint_accept(m_endpoint)))
                ;

            dispatchListenChannel(m_peers[i].chan);
        }

        m_peers[m_id].addr = m_addr;
        m_peers[m_id].chan = NULL;

        //  for(int i = m_id + 1; i < m_num_transports; i++)
        for(int i = m_num_transports - 1; i > m_id; i--)
        {
            // Initiate endpoint_connect() with all CMs with id's higher than us
            while(!name_lookup(CONN_MANAGER_NAME + i + 1, &m_peers[i].addr))
                ;

            m_peers[i].chan = endpoint_connect(m_endpoint,
                    m_peers[i].addr);

            dispatchListenChannel(m_peers[i].chan);
        }

        dispatchListen(m_endpoint);

    }

    void ConnManagerTransportLink::establishIncomingConn(Channel *chan,
            transport::flow_t listen_sock_id, transport::flow_t flow_id, 
            in_addr_t source, uint16_t source_port,
            Address acceptor, bool acceptor_valid,
            void *syn, size_t syn_len)
    {
        m_flow_cm_map.insert(std::make_pair<transport::flow_t, Channel *>(flow_id, chan));
        m_transport->incomingConn(listen_sock_id, flow_id, source, source_port, acceptor, acceptor_valid, syn, syn_len);
    }

    void ConnManagerTransportLink::handleTimeout(transport::flow_t flow_id, Address app)
    {
        // FIXME: Not implemented yet
    }

    FosStatus ConnManagerTransportLink::bindAndListen(struct in_addr addr, uint16_t port, bool shared, bool notify_peers, transport::flow_t *listen_sock_id)
    {
        // FIXME: When is this needed? If it is indeed needed, we should
        // reenable it 
        // This is required to fix a bug where two processes could
        // request to bindAndListen at the same time and both find that nobody
        // is listening on a given port instead of sharing
        //
        // threading::ScopedTicketLock sl(m_bind_lock);
        //

        uint16_t nport = ntohs(port);
        bool is_inaddr_any = (addr.s_addr == 0);

        if(is_inaddr_any) //remap 0.0.0.0 to 127.0.0.1, and we add the public ip address lower
            *listen_sock_id = JSNetHash(ntohl(0x7F000001), nport, 0, 0);
        else
            *listen_sock_id = JSNetHash(addr.s_addr, nport, 0, 0);

        // First see if we need to return an already listening socket...
        if(shared && m_listen_map.find(*listen_sock_id) != m_listen_map.end())
            return FOS_MAILBOX_STATUS_OK;

        transport::flow_t listen_sock_id_2;
        if(is_inaddr_any)
        {
            listen_sock_id_2 = JSNetHash(m_transport->getIP(), nport, 0, 0);
        }

        FosStatus status = createListenSocket(*listen_sock_id, listen_sock_id_2, is_inaddr_any);

        if((status == FOS_MAILBOX_STATUS_OK)
                && notify_peers)
        {
            BindAndListenRequest *req;
            BindAndListenReply *reply;
            DispatchTransaction trans;
            for(int i = 0; i < m_peers.size(); i++)
            {
                if(i == m_id) continue;
                Channel *chan = m_peers[i].chan;

                req = (BindAndListenRequest *)
                    dispatchAllocate(chan, sizeof(*req));

                req->port = port;
                req->addr = addr;
                req->shared = shared;

                trans = dispatchRequest(chan, PEER_BIND_AND_LISTEN, req, sizeof(*req));

                dispatchReceive(trans, (void **) &reply);

                // FIXME: Notify all the peers who have already set up a listen
                // socket to take it down
                if(reply->status != FOS_MAILBOX_STATUS_OK)
                    return reply->status;
            }
        }

        return status;
    }

    Address ConnManagerTransportLink::incomingConn(in_addr_t source, uint16_t source_port,
            in_addr_t dest, uint16_t dest_port,
            void *syn, size_t syn_len)
    {
        transport::flow_t listen_sock_id = JSNetHash(dest, dest_port, 0, 0);

        ListenMap::iterator it = m_listen_map.find(listen_sock_id);

        if(it == m_listen_map.end())
        {
//            fprintf(stderr, "no listening socket found at dest port: %d for listen id: %x\n", dest_port, listen_sock_id);
            // No listening socket found. Drop packet?
            return FOS_MAILBOX_STATUS_RESOURCE_UNAVAILABLE;
        }

        ListenSocket *listen = it->second;
        transport::flow_t flow_id = JSNetHash(dest, dest_port, source, source_port);
        Address acceptor;

        bool acceptor_valid = listen->getAcceptor(&acceptor);

        if(m_transport->echoPort() && m_transport->echoPort() == dest_port)
        {
            acceptor_valid = true;
            acceptor = 0;
        }


        // Forward the incoming connection request to a transport server to set
        // up the connection
        Channel *ts_chan;
        Address trans = chooseTransport(&ts_chan);
        bool local = (trans == m_transport->getAddr());

        if(local)
        {
            m_transport->incomingConn(source, source_port, listen_sock_id, flow_id, acceptor, acceptor_valid, syn, syn_len);
        }
        else
        {
            ConnectionAcceptedEventTransportSyn *tr_req = (ConnectionAcceptedEventTransportSyn *)
                dispatchAllocate(ts_chan, sizeof(*tr_req) + syn_len);

            tr_req->listen_sock_id = listen_sock_id;
            tr_req->flow_id = flow_id;
            tr_req->addr = source;
            tr_req->port = source_port;
            tr_req->acceptor = acceptor;
            tr_req->acceptor_valid = acceptor_valid;
            tr_req->syn_len = syn_len;
            memcpy(tr_req->syn, syn, syn_len);

            dispatchRequest(ts_chan, SYN, tr_req, sizeof(*tr_req) + syn_len);
        }

        if(!acceptor_valid)
        {
            // Save pending connection
            Connection pending;
            pending.connection_id = flow_id;
            if(local)
                pending.netstack = NULL;
            else
                pending.netstack = ts_chan;

            listen->pushPendingConnection(pending);
        }

        return trans;
    }

    FosStatus ConnManagerTransportLink::accept(transport::flow_t listen_sock_id, Address app,
            bool *conn_available)
    {
        // Get the socket
        ListenMap::iterator it = m_listen_map.find(listen_sock_id);
        if(it == m_listen_map.end())
        {
            *conn_available = false;
            return FOS_MAILBOX_STATUS_RESOURCE_UNAVAILABLE;
        }

        ListenSocket *listen_sock = it->second;

        // Try to find a pending connection to wake up, keep trying until one is unclaimed
        while (true)
        {
            Connection pending_conn;
            bool is_connection_pending = listen_sock->popPendingConnection(&pending_conn);

            if (!is_connection_pending)
            {
                listen_sock->addAcceptor(app, true);
                break;
            }
            else
            {
                // We are about to hand over a connection to this acceptor, so
                // mark it as unlikely to accept another one soon
                listen_sock->addAcceptor(app, false);
            }

            // Pending connection found!
            // 
            // Notify the appropriate transport that a new acceptor is
            // ready for this connection; this starts the three-way handshake
            // -- this will bypass the TCP set up (which has already been
            // done) and start the timeout loop to get someone to
            // eventually accept the connection

            bool is_claimed;
            bool is_closed;
            Address oops_valid_app;
            if(pending_conn.netstack == NULL)
            {
                m_transport->provideAppWithConnection(pending_conn.connection_id, app, 
                        &is_closed, &is_claimed, &oops_valid_app);
            }
            else
            {
                Channel *ts_chan = pending_conn.netstack;
                ConnectionAcceptedEventTransport *ts_req = (ConnectionAcceptedEventTransport *)
                    channel_allocate(ts_chan, sizeof(*ts_req));
                DispatchTransaction trans = dispatchRequest(ts_chan, PROVIDE_APP_WITH_CONNECTION, ts_req, sizeof(*ts_req));

                ConnectionAcceptedEventTransportReply *ts_reply;
                dispatchReceive(trans, (void **) &ts_reply);
                is_claimed = ts_reply->already_claimed;
                is_closed = ts_reply->is_closed;
                oops_valid_app = ts_reply->oops_valid_app;
            }

            if(!(is_closed || is_claimed))
            {
                // Found a connection!
                *conn_available = true;
                return FOS_MAILBOX_STATUS_OK;
            }

            if(is_claimed)
                listen_sock->addAcceptor(oops_valid_app, false);
        }

        // No connection is available   :(
        *conn_available = false;
        return FOS_MAILBOX_STATUS_OK;
    }

    /* Private methods */

    FosStatus ConnManagerTransportLink::createListenSocket(const transport::flow_t &listen_sock_id,
            const transport::flow_t &listen_sock_id_2, bool is_inaddr_any)
    {
        // TODO: This should be freed on socket close()
        ListenSocket *listen_socket = new ListenSocket();

        if(!listen_socket)
            return FOS_MAILBOX_STATUS_ALLOCATION_ERROR;

        listen_socket->setConnectionId(listen_sock_id);

        m_listen_map.insert(std::pair<transport::flow_t, ListenSocket *>(listen_sock_id, listen_socket));

        if(is_inaddr_any)
        {
            m_listen_map.insert(std::pair<transport::flow_t, ListenSocket *>(listen_sock_id_2, listen_socket));
        }

        return FOS_MAILBOX_STATUS_OK;
    }

    Address ConnManagerTransportLink::chooseTransport(Channel **chan)
    {
        if(m_curr_transport == m_transport->id())
        {
            *chan = NULL;
            return m_transport->getAddr();
        }

        Address addr; 
        Name name = TRANSPORT_NAME + m_curr_transport;
        name_lookup(name, &addr);
        *chan = dispatchOpen(addr);

        m_curr_transport = (m_curr_transport + 1) % m_num_transports;

        return addr;
    }

}}} // namespaces
