#pragma once

#include <messaging/channel.h>
#include <messaging/dispatch.h>
#include "transport_private.hpp"

// @file transport_socket.hpp
// @brief This file contains structures for different types of
// connections that can be active in the transport.
// 
// As opposed to socket.hpp, which contains a Socket class that lives
// in the application.

namespace fos { namespace net {

namespace transport {

    typedef unsigned int flow_t;

    // Long-lived data cached in lwip

    // Different types of sockets
    enum SocketType {
        FLOW_SOCKET = 0x1,
        PENDING_FLOW_SOCKET = 0x2,
        CONNECT_SOCKET = 0x3,
        MESSAGE_SOCKET = 0x4,
    };

    // Generic base class to let us catch when requests go to the
    // wrong type of socket
    class PcbCallback
    {
    public:
        PcbCallback(SocketType type) : m_type(type) {}
        SocketType type() const { return m_type; }

    private:
        SocketType m_type;
    };

    // Data associated with a live connection; if its an incoming
    // flow, then FlowSocket will be used, otherwise ConnectSocket
    // (which derives from this) provides some extra info
    struct FlowSocket : public PcbCallback
    {
    public:

        enum SocketState
        {
            ESTABLISHED,
            CLOSE_WAIT, // State entered when the remote application resets the connection
            LAST_ACK, // State entered when the local app calls close in response to a remote reset
            TIME_WAIT, // State entered when the local application initiates close()
        };

        FlowSocket() : PcbCallback(FLOW_SOCKET), m_state(ESTABLISHED), m_claimed(false), m_flow_id(0), m_pending_delivery(false) {}
        FlowSocket(SocketType type) : PcbCallback(type), m_state(ESTABLISHED),  m_claimed(false), m_flow_id(0), m_pending_delivery(false) {}

        void setConnectionId(transport::flow_t flow_id) { m_flow_id = flow_id; }
        transport::flow_t getFlowId() { return m_flow_id; }

        struct ChanPair
        {
            Channel *async;
            Channel *sync;
        };

        Address m_client_addr;
        ChanPair m_client;
        SocketState m_state;
        bool m_claimed;
        // FIXME: Also have local ip and port
        in_addr_t m_remote_ip;
        uint16_t m_remote_port;
        transport::flow_t m_flow_id;
        transport::flow_t m_listen_socket_id;
        // threading::TicketLock m_ticket_lock; // control re-ordering of data
        // threading::TicketLock m_recv_ticket_lock; // control re-ordering of data
        bool m_pending_delivery;
    };

    // Info associated with an outgoing connection
    struct ConnectSocket : public FlowSocket
    {
        ConnectSocket() : FlowSocket(CONNECT_SOCKET) { }
        // used to respond to app after a connection is accepted -nzb
        Channel* m_chan;
        DispatchTransaction m_trans;
    };

    // Info associated with a flow connection which hasn't been fully set up yet
    // i.e., we have received the incoming conn request from CM but haven't 
    // completed the TCP three way handshake with the remote end yet
    //
    // m_acceptor_valid = false indicates that the CM has not passed us a
    // valid acceptor for this flow yet, this flow should thus be added to
    // the list of pending flows in acceptCallback() waiting for the CM to
    // notify us of an acceptr
    //
    // m_cm_rbox is the remotebox for the CM that passed us this flow and is
    // used to notify the CM of app timeouts if they happen so we can get a new
    // acceptor
    struct PendingFlowSocket : public PcbCallback
    {
        public:
            PendingFlowSocket() : PcbCallback(PENDING_FLOW_SOCKET) {}

            in_addr_t m_remote_ip;
            uint16_t m_remote_port;
            flow_t m_flow_id;
            flow_t m_listen_sock_id;

            bool m_acceptor_valid;
            Address m_acceptor_addr;

            // FIXME: Need some information about which CM to talk to in case of
            // error
            // Address m_cm_addr; 
    };
}

namespace conn {

    // @see conn_socket.hpp for the ListenSocket declaration
    class ListenSocket;
    class MessageSocket;
}

namespace transport {

    // Conversion functions
    template<class T>
    T * cast(void *);

    template<>
    inline FlowSocket * cast(void * vp)
    {
        PcbCallback * p = (PcbCallback *)vp;
        if (!(p->type() & FLOW_SOCKET)) return NULL;
        return (FlowSocket *) vp;
    }

    template<>
    inline ConnectSocket * cast(void * vp)
    {
        PcbCallback * p = (PcbCallback *)vp;
        if (p->type() != CONNECT_SOCKET) return NULL;
        return (ConnectSocket *) vp;
    }

}}}
