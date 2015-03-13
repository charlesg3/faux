#pragma once

#include <map>
#include <vector>
#include <messaging/dispatch.h>
#include "conn_private.hpp"
#include "conn_socket.hpp"
#include "transport_socket.hpp"

// #include <dispatch/dispatch.h>
// #include <threading/ticket_lock.hpp>

// Forward declaration of Transport
namespace fos { namespace net { namespace transport {
class Transport;
}}}

namespace fos { namespace net { namespace conn {

    class ConnLink
    {
        protected:
            static ConnLink *g_conn_link;
        public:
            static inline ConnLink & connLink() { return *g_conn_link; };
            virtual Address incomingConn(in_addr_t source, uint16_t source_port,
                              in_addr_t dest, uint16_t dest_port,
                              void *syn, size_t syn_len) = 0;
    };

    class ConnTransport
    {
        protected:
            static ConnTransport *g_conn_transport;
        public:
            static inline ConnTransport & connTransport() { return *g_conn_transport; };
            virtual void handleTimeout(transport::flow_t flow_id, Address app) = 0;
            virtual void establishIncomingConn(Channel *chan,
                    transport::flow_t listen_sock_id, transport::flow_t flow_id, 
                    in_addr_t source, uint16_t source_port,
                    Address acceptor, bool acceptor_valid,
                    void *syn, size_t syn_len) = 0;
    };

    class ConnManager
    {
        protected:
            static ConnManager *g_conn_manager;
        public:
            static inline ConnManager & connManager() { return *g_conn_manager; };
            virtual FosStatus bindAndListen(struct in_addr addr, uint16_t port, bool shared, bool notify_peers, transport::flow_t *listen_sock_id) = 0;
            virtual Address incomingConn(in_addr_t source, uint16_t source_port,
                              in_addr_t dest, uint16_t dest_port,
                              void *syn, size_t syn_len) = 0;
            virtual FosStatus accept(transport::flow_t listen_sock_id, Address app,
                    bool *conn_available) = 0;
            virtual void handleTimeout(transport::flow_t flow_id, Address app) = 0;
    };


    class ConnManagerTransportLink : public ConnManager, public ConnTransport, public ConnLink
    {
        private:
            struct Peer
            {
                Peer(Address address, Channel *channel)
                    : addr(address)
                    , chan(channel) {}
                Address addr;
                Channel *chan;
            };

            typedef std::map<transport::flow_t, conn::ListenSocket *> ListenMap;
            typedef std::map<transport::flow_t, Channel *> FlowCMMap;
            Endpoint *m_endpoint;
            Address m_addr;

        public:
            ConnManagerTransportLink(transport::Transport *trans, int id, int num_transports);
            ~ConnManagerTransportLink();

            void establishIncomingConn(Channel *chan,
                    transport::flow_t listen_sock_id, transport::flow_t flow_id, 
                    in_addr_t source, uint16_t source_port,
                    Address acceptor, bool acceptor_valid,
                    void *syn, size_t syn_len);

            void handleTimeout(transport::flow_t flow_id, Address app);

            FosStatus bindAndListen(struct in_addr addr, uint16_t port, bool shared, bool notify_peers, transport::flow_t *listen_sock_id);
            Address incomingConn(in_addr_t source, uint16_t source_port,
                    in_addr_t dest, uint16_t dest_port,
                    void *syn, size_t syn_len);
            FosStatus accept(transport::flow_t listen_sock_id, Address app,
                    bool *conn_available);

        private:

            FosStatus createListenSocket(const transport::flow_t &listen_sock_id,
                    const transport::flow_t &listen_sock_id_2, bool is_inaddr_any);
            Address chooseTransport(Channel **chan);

            transport::Transport *m_transport;
            Address m_link_addr; // These will have to be a vector/list 
            Channel *m_link_chan;
            ListenMap m_listen_map;
            FlowCMMap m_flow_cm_map;
            std::vector<struct Peer> m_peers;
            int m_num_transports;
            int m_id;
            int m_curr_transport;
    };

}}} // namespaces

