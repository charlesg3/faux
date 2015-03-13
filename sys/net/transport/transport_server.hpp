#pragma once

#include <map>
#include <list>

#include <lwip/tcp.h>
#include <lwip/udp.h>
#include <lwip/err.h>

//#include <threading/ticket_lock.hpp>
#include "transport_private.hpp"
#include "conn_listen.hpp"
#include "conn_transport.hpp"
#include "conn_manager.hpp"
#include "conn_marshall.hpp"

namespace fos { namespace net { namespace transport {

    class FlowSocket;
    class ConnectSocket;

    class Transport
    {
    public:
        Transport(struct netif *nif, int id, int num_transports);
        ~Transport() {};

        static Transport & transport() { return *g_transport; }

        void go();

        // echo support
        void startEcho(int port);
        void echoAccept(tcp_pcb *pcb);
        int echoPort();

        void incoming(void * data, size_t len);
        void incomingBroadcast(void * data, size_t len); //Used to forward packets to all child netstacks, for ARP replies
        inline Address getAddr() { return m_address; };
        inline Endpoint *getEndpoint() { return m_endpoint; };
        /*
        void transferData(void * data, size_t len);

        void idle();

        Status establishConnection(struct tcp_pcb * pcb, flow_t hash);
        FosStatus bind(messaging::Remotebox & listen, uint16_t port, struct in_addr addr, bool shared, flow_t * connection_id);
        */
        void connect(Channel* chan, DispatchTransaction trans, Address client_addr, struct in_addr addr, uint16_t port);
        ssize_t write(flow_t connection_id, const void * data, size_t len);
        FosStatus close(flow_t connection_id);
        /*
        ssize_t sendTo(flow_t connection_id, const void * data, size_t len, struct in_addr dest, uint16_t port);
        FosStatus getPeer(flow_t connection_id, in_addr_t * ip);
        FosStatus getHostByName(messaging::Remotebox & rbox, const char * hostname, struct in_addr * addr, void * callback_data);
        FosStatus netShutdown();

        void recvMessageCallback(FlowSocket * arg, struct udp_pcb * pcb, struct pbuf * p, struct in_addr *addr, uint16_t port);
        */
        err_t recvCallback(FlowSocket * arg, struct tcp_pcb * pcb, struct pbuf * p, err_t err);
        err_t connectCallback(ConnectSocket *arg, struct tcp_pcb *tpcb, err_t err);
        void finishedCallback(FlowSocket * flow, struct tcp_pcb *tpcb);
        void ifUpCallback(in_addr_t ip_addr, in_addr_t netmask, in_addr_t gw);

        void closeConnection(FlowSocket * flow_data);
        void incomingConn(in_addr_t source, uint16_t source_port,
                flow_t listen_sock_id, flow_t flow_id,
                Address acceptor, bool acceptor_valid,
                void *syn, size_t syn_len);
        void provideAppWithConnection(
                transport::flow_t &flow_id, 
                Address acceptor,
                bool *is_closed,
                bool *is_claimed,
                Address *oops_valid_app);
        // void handleTimeout(flow_t flow_id, messaging::Remotebox acceptor);

        //void linkSend(const void * data, size_t len);

        int id() { return m_id; }
        FosStatus setIP(in_addr_t ip_addr, in_addr_t sn_mask, in_addr_t gw);
        in_addr_t getIP() { return m_ip_addr; };

    private:
        friend void claimConnection(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
        friend err_t acceptCallback(Transport * transport, PendingFlowSocket *pending_flow, struct tcp_pcb * pcb, err_t err);

        bool isClaimed(flow_t connection_id, FlowSocket ** flow);
        FlowSocket * getFlowSocket(flow_t connection_id);
        void pushRefusedPackets(flow_t connection_id);

        Endpoint *m_endpoint;
        Address m_address;
        int m_num_transports;
        struct netif * m_netif;
        static Transport * g_transport;
        conn::ConnTransport *m_conn;

        struct timeval m_last_arp_time;
        struct timeval m_last_dns_time;
        struct timeval m_last_dhcp_fine_time;
        struct timeval m_last_dhcp_coarse_time;

        class FlowMap
        {
        public:
            struct tcp_pcb * get(flow_t connection_id);
            flow_t compute(struct tcp_pcb * pcb);
            flow_t compute(struct tcp_pcb_listen * pcb);
            flow_t compute(struct udp_pcb * pcb);
            flow_t add(struct tcp_pcb * pcb);
            flow_t add(struct udp_pcb * pcb);
            void remove(flow_t connection_id);
            bool contains(flow_t id);

        private:
            typedef std::map<flow_t, struct tcp_pcb * > InternalFlowMap;
            InternalFlowMap m_flows;
        };

        FlowMap m_flows;
        /*
        threading::TicketLock m_bind_lock;
        threading::TicketLock m_received_data_lock;
        */

        int m_id;

        // Used by the TS to perform the asynchronous part of the connection
        // handoff (i.e. the first part of the 3-way handshake for incoming
        // connections)
        std::map<Address, Channel *> m_conn_handoff_chans;

        // Connection to the children transports, this is only used
        // by the master transport to propagate the ip address to the 
        // children transports
        Channel ** m_child_transports;

        in_addr_t m_ip_addr;
    };

}}}
