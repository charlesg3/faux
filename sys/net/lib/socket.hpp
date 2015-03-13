#pragma once

#include <map>
#include <sys/time.h>
#include <sys/net/transport.h>
#include <system/status.hpp>
#include "./conn_lib.hpp"
#include "./transport_private.hpp"
#include "messaging/dispatch.h"

namespace fos { namespace net { namespace app {

    struct ChanPair
    {
        Channel *async;
        Channel *sync;
    };

    class Socket
    {
    public:
        enum State
        {
            OPEN,
            CLOSED,
            LISTEN,
            DRAINING
        };

        enum SocketType
        {
            LISTEN_SOCKET,
            FLOW_SOCKET
        };

    public:
        Socket(int domain, int type, int protocol, SocketType sock_type);
        ~Socket();

        Status getPeer(struct sockaddr *out_address);

        FosStatus recvFrom(void *mem, size_t len, int flags,
                struct sockaddr * __restrict from, socklen_t * __restrict fromlen);

        FosStatus sendTo(const void *dataptr, size_t size, int flags,
                const struct sockaddr *to, socklen_t tolen);

        virtual bool poll() = 0;
        virtual Status close() = 0;
        Status stat();

        State state() { return m_state; }
        SocketType getType() { return m_socket_type; }

        int getDomain() { return m_domain; }
        int getPType() { return m_type; }
        int getProtocol() { return m_protocol; }

        static void dumpStats();

    protected:
        State m_state;
        int m_domain;
        int m_type;
        int m_protocol;

        struct sockaddr m_name;
        transport::flow_t m_flow_id;

        SocketType m_socket_type;

    private:
//        threading::TicketLock m_ticket_lock;
    };

    class FlowSocket : public Socket
    {
    public:
        FlowSocket(int domain, int type, int protocol)
            : Socket(domain, type, protocol, FLOW_SOCKET)
            , m_nbuffered(0)
        {
            m_ts_chans = {NULL, NULL}; // Not established yet
        }

        virtual Status close();
        ssize_t read(void *buf, size_t count, bool blocking);
        ssize_t write(const void *buf, size_t count);
        Status connect(const struct sockaddr *in_address, socklen_t namelen);
        virtual bool poll();

        static inline Address getFlowAddress() { init(); return g_flow_addr; }
        static inline Endpoint * getFlowEndpoint() { init(); return g_flow_endpoint; }
    private:
        ssize_t copyFromBuffer(void *buf, size_t count);

        static const size_t BUFFER_SIZE = 64 * 1024 * 10;
        ChanPair m_ts_chans; // Used by flow sockets
        char m_buffer[BUFFER_SIZE];
        uint64_t m_nbuffered;

        friend class ListenSocket;

        static void init();     // delayed constructor to deal with
                                // shared endpoints across a fork() -nzb

        static Address g_flow_addr;
        static Endpoint * g_flow_endpoint;
    };

    class ListenSocket : public Socket
    {
    public:
        ListenSocket(int domain, int type, int protocol)
            : Socket(domain, type, protocol, LISTEN_SOCKET)
            , m_cm_chan(0)
            , m_last_conn_update(0)
            , m_shared(false)
        {
        }

        Status bind(const struct sockaddr *name, socklen_t namelen);
        Status listen(int backlog);
        Status accept(void **vp, struct sockaddr * in_address, socklen_t * in_address_len, bool blocking);
        virtual Status close();

        ChanPair pollListenChans(bool *conn_ready);
        ChanPair setAccepting(bool *conn_available);
        bool acceptConnection(FlowSocket * & connection_socket, 
                const conn::ConnectionAcceptedEventApp * event, Channel *chan);

        int setShared(bool shared);
        virtual bool poll();

    private:
        typedef std::map<Address, ChanPair> ListenChanMap;
        ListenChanMap m_listen_chans; // Used by listen sockets
        Channel *m_cm_chan; // Used by listen sockets
        uint64_t m_last_conn_update;
        bool m_shared;

        void init();

        inline Address getAcceptAddress() { init(); return g_accept_addr; }
        inline Endpoint * getAcceptEndpoint() { init(); return g_accept_endpoint; }

        Address g_accept_addr;
        Endpoint *g_accept_endpoint;
    };

}}}
