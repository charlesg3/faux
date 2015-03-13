#include <vector>
#include <netif/etharp.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <utilities/debug.h>
#include <utilities/time_arith.h>
#include <utilities/tsc.h>
#include <rpc/rpc.h>
#include <lwip/err.h>
#include <config/config.h>
#include <name/name.h>
#include <messaging/dispatch.h>
#include <messaging/channel.h>

#include "socket.hpp"
#include "conn_private.hpp"
#include "transport_private.hpp"

#include <assert.h>

namespace fos { namespace net { namespace app {

#define min(a,b) (((a) < (b)) ? (a) : (b))

Address FlowSocket::g_flow_addr;
Endpoint * FlowSocket::g_flow_endpoint;

Socket::Socket(int domain, int type, int protocol, SocketType sock_type)
    : m_state(CLOSED)
    , m_domain(domain)
    , m_type(type)
    , m_protocol(protocol)
    , m_flow_id(0)
    , m_socket_type(sock_type)
{
    //FIXME: get udp names
    //Initialize the netstack we are talking to right away if it's a dgram type socket
    if(m_type == SOCK_DGRAM)
    {
        // Not supported right now
        assert(false);
    }
}

Socket::~Socket()
{
}

void FlowSocket::init() {
    if (!g_flow_endpoint) {
        g_flow_endpoint = endpoint_create();
        g_flow_addr = endpoint_get_address(g_flow_endpoint);
    }
}

void ListenSocket::init() {
    if (g_accept_endpoint == NULL) {
        g_accept_endpoint = endpoint_create();
        g_accept_addr = endpoint_get_address(g_accept_endpoint);
    }
}

int ListenSocket::setShared(bool shared)
{
    m_shared = shared;
    return 0;
}

Status FlowSocket::connect(const struct sockaddr *in_address, socklen_t namelen)
{
    transport::ConnectRequest *req;
    DispatchTransaction trans;
    transport::ConnectReply *reply;
    __attribute__((unused)) size_t size;

    uint16_t port = ((struct sockaddr_in *)(in_address))->sin_port;

    Address transport_addr;

//    PS("looking up transport name.");
    while(!name_lookup(TRANSPORT_NAME, &transport_addr))
        ;

    m_ts_chans.sync = dispatchOpen(transport_addr);

    req = (transport::ConnectRequest *) dispatchAllocate(m_ts_chans.sync, sizeof(*req));

    req->client_addr = getFlowAddress();
    req->addr = ((struct sockaddr_in *)(in_address))->sin_addr;
    req->port = ((struct sockaddr_in *)(in_address))->sin_port;

    trans = dispatchRequest(m_ts_chans.sync, transport::CONNECT, req, sizeof(transport::ConnectRequest));

    size = dispatchReceive(trans, (void **) &reply);
    assert(size == sizeof(transport::ConnectReply));

    if(reply->status == FOS_MAILBOX_STATUS_OK)
    {
        m_flow_id = reply->connection_id;
        m_state = OPEN;
    }

    FosStatus ret = reply->status;

    dispatchFree(trans, reply);

    while((m_ts_chans.async = endpoint_accept(getFlowEndpoint())) == NULL)
        ;

    return ret;
}

Status ListenSocket::bind(const struct sockaddr *name, socklen_t namelen)
{
    if(m_type != SOCK_DGRAM)
    {
        m_name = *name;
        return FOS_MAILBOX_STATUS_OK; //we lazy bind for tcp/ip connections on listen()
    }
    
    // FIXME: Not implemented yet
    return FOS_MAILBOX_STATUS_GENERAL_ERROR;

#if 0
    m_name = *name;

    // Begin rpc boilerplate code
    BindRequest req = {
        DispatchMessageHeader(),
        *rpcGetResponseMailbox(),
        m_self,
        ((struct sockaddr_in *)&m_name)->sin_port,
        ((struct sockaddr_in *)&m_name)->sin_addr,
        m_shared,
    };

    //FIXME: get udp names
    m_netstack.ref().alias = Alias("/sys/net/tcp_listen").ref();

    if(fosEnvLookupMailboxCapability(&m_netstack.capability(), fosEnv(), &m_netstack.ref().alias) != FOS_MAILBOX_STATUS_OK)
    {
        m_netstack.ref().alias = Alias("/sys/net/@local/tcp_listen").ref();
        if(fosEnvLookupMailboxCapability(&m_netstack.capability(), fosEnv(), &m_netstack.ref().alias) != FOS_MAILBOX_STATUS_OK)
            return FOS_MAILBOX_STATUS_INVALID_ALIAS_ERROR;
    }

    DispatchToken token = dispatchSendRequest(&m_netstack.alias(), m_netstack.capability(),
            &req, sizeof(req), BIND);

    DispatchResponse *rc = rpcWaitForResponse(token);

    BindReply *reply = (BindReply *)rc->data;
    m_flow_id = reply->connection_id;

    FosStatus ret = reply->status;
    dispatchResponseFree(rc);
    // END rpc boilerplate code

    if(ret == FOS_MAILBOX_STATUS_OK)
        m_state = OPEN;

    return ret;
#endif
}

FosStatus Socket::recvFrom(void *mem, size_t len, int flags,
        struct sockaddr * __restrict from, socklen_t * __restrict fromlen)
{
    // FIXME: Not implemented yet
    return FOS_MAILBOX_STATUS_GENERAL_ERROR;
#if 0
    if (m_state == CLOSED || m_type != SOCK_DGRAM)
    {
        errno = EBADF;
        return -1;
    }

    Mailbox::Message msg = m_mailbox.recv();

    ReceivedMessageEvent & evt = msg.ref<ReceivedMessageEvent>();

    size_t maxlen = len > evt.len ? evt.len : len;

    memcpy(mem, evt.data, maxlen);

//    assert(*fromlen == sizeof(struct in_addr) + sizeof(uint16_t));

    struct sockaddr_in *from_p = (struct sockaddr_in *)from;

    from_p->sin_family = AF_INET;
    from_p->sin_port = evt.port;
    from_p->sin_addr = evt.addr;

    return  maxlen;
#endif
}

FosStatus Socket::sendTo(const void *dataptr, size_t size, int flags,
        const struct sockaddr *to, socklen_t tolen)
{
    // FIXME: Not implemented yet
    return FOS_MAILBOX_STATUS_GENERAL_ERROR;
#if 0
    FosStatus ret = size;

    // Attempting to sendto on connection-mode socket
    // If to and tolen are not 0, then we should set errno=EISCONN
    // and return -1, but some applications (like memcached) depend
    // on that case being ignored and reverting to send-like behavior
    if(m_type != SOCK_DGRAM)
    {
        return this->write(dataptr, size);
    }

    assert(m_type == SOCK_DGRAM);

    if(m_flow_id == 0)
    {
        struct sockaddr_in si_me;
        si_me.sin_family = AF_INET;
        si_me.sin_port = 0;
        si_me.sin_addr.s_addr = 0;
        bind((struct sockaddr *)&si_me, sizeof(struct sockaddr_in));
    }

    // Begin rpc boilerplate code
    SendToRequest req;
    req.rbox = *rpcGetResponseMailbox();
    req.flags = flags;
    req.connection_id = m_flow_id;
    req._size = size;

    struct sockaddr_in *dest = (struct sockaddr_in *)to;

    req.addr = dest->sin_addr;
    req.port = dest->sin_port;

    struct iovec iov[2];
    iov[0].iov_base = &req;
    iov[0].iov_len = sizeof(req);
    iov[1].iov_base = (void *)dataptr;
    iov[1].iov_len = size;

    dispatchSendRequestV(&m_netstack.alias(), m_netstack.capability(),
            iov, 2, SEND_TO);

    return ret;
#endif
}

Status ListenSocket::listen(int backlog)
{
    if(!m_cm_chan)
    {
        Address cm_addr;
        name_lookup(CONN_MANAGER_NAME, &cm_addr);
        m_cm_chan = dispatchOpen(cm_addr);
    }

    conn::BindAndListenRequest *req;
    DispatchTransaction trans;
    conn::BindAndListenReply *reply;
    __attribute__((unused)) size_t size;

    req = (conn::BindAndListenRequest *) dispatchAllocate(m_cm_chan, sizeof(conn::BindAndListenRequest));

    req->port = ((struct sockaddr_in *)&m_name)->sin_port;
    req->addr = ((struct sockaddr_in *)&m_name)->sin_addr;
    req->shared = m_shared;

    trans = dispatchRequest(m_cm_chan, conn::BIND_AND_LISTEN, req, sizeof(conn::BindAndListenRequest));

    size = dispatchReceive(trans, (void **) &reply);
    assert(size == sizeof(conn::BindAndListenReply));

    m_flow_id = reply->listen_sock_id;
    FosStatus ret = reply->status;

    dispatchFree(trans, reply);

    m_state = LISTEN;

    return ret;
}

Status ListenSocket::accept(void **vp, struct sockaddr * out_address, socklen_t * out_address_len, bool blocking)
{
    assert(m_state == LISTEN);

    // Try to be a little smart about only creating connection socket
    // when needed, and only creating one at most
    FlowSocket * connection_socket = NULL;

    while (true)
    {
        // notify our listening netstack that we want to accept a connection
        ChanPair chans;
        bool conn_available = false;
        chans = setAccepting(&conn_available);

        void *msg;
        size_t size;
        if (blocking)
        {
            if(!conn_available)
            {
                chans = pollListenChans(&conn_available);
                if(!conn_available)
                    continue;
            }
        }
        else
        {
            if(!conn_available)
                chans = pollListenChans(&conn_available);

            if(!conn_available)
                return FOS_MAILBOX_STATUS_EMPTY_ERROR;
        }

        channel_receive_blocking(chans.async, &msg);

        conn::ConnectionAcceptedEventApp *evt = (conn::ConnectionAcceptedEventApp *)msg;

        // Try to accept connection; retry if we fail
        if (!acceptConnection(connection_socket, evt, chans.sync))
        {
            PS("oops we didn't get it.");

            // FIXME: This should be blocking || a connection waiting
            // if (blocking || channel_poll(m_transport)) 
            // If we are blocking stay in the loop, or if we are
            // non-blocking, make sure to drain all messages
            delete connection_socket;
            channel_free(chans.async, msg);

            if(blocking)
                continue;
            else
                return FOS_MAILBOX_STATUS_EMPTY_ERROR;
        }

        if(out_address_len)
        {
            if (out_address &&  *out_address_len >= sizeof(struct sockaddr))
            {
                //FIXME: shouldn't we set the port here too? --cg3
                memset(out_address, 0, sizeof(struct sockaddr));
                ((struct sockaddr_in *)&out_address)->sin_addr.s_addr = evt->addr;
                *out_address_len = sizeof(struct sockaddr);
            }
            else
            {
                *out_address_len = 0;
            }
        }

        // vp must point to new socket for connection
        *vp = (void *)connection_socket;

        channel_free(chans.async, msg);
        return FOS_MAILBOX_STATUS_OK;
    }

    return FOS_MAILBOX_STATUS_OK;
}

Status ListenSocket::close()
{
    USE_TRACE_NETSTACK(dumpStats());
    FosStatus ret = FOS_MAILBOX_STATUS_OK;

    // FIXME: Send an update to the CM here)
    // FIXME: should probably only need to close
    // channels we have open. See constructor (Socket())
    //dispatchClose(m_cm_chan);
    // dispatchIgnoreChannel(m_ts_chan);
    // channel_close(m_ts_chan);
    if(m_state == LISTEN)
        channel_close(m_cm_chan);

    return ret;
}

Status FlowSocket::close()
{
    USE_TRACE_NETSTACK(dumpStats());
    FosStatus ret = FOS_MAILBOX_STATUS_OK;

    // If the close is coming from the app end, tell the netstack. If the close came
    // from the remote end, m_state was set to CLOSED in read().
    // if (m_state != CLOSED)
    if(m_state == OPEN || m_state == DRAINING)
    {
        // Begin rpc boilerplate code
        transport::CloseRequest *req = (transport::CloseRequest *)dispatchAllocate(m_ts_chans.sync, sizeof(*req));
        req->flow_id = m_flow_id;

        DispatchTransaction trans = dispatchRequest(m_ts_chans.sync, transport::CLOSE, req, sizeof(req));

        transport::CloseReply *reply;
        size_t size = dispatchReceive(trans, (void **)&reply);

        ret = reply->status;
        dispatchFree(trans, reply);
        // END rpc boilerplate code

        m_state = CLOSED;

        //dispatchClose(m_cm_chan);
        dispatchIgnoreChannel(m_ts_chans.sync);
        channel_close(m_ts_chans.sync);
        channel_close(m_ts_chans.async);
    }

    return ret;
}

bool FlowSocket::poll()
{
    switch(m_state)
    {
        case CLOSED:
            return false;
        case DRAINING:
            return m_nbuffered > 0;
        case OPEN:
            break;
        case LISTEN:
        default:
            assert(false);
    }

    // Channel is valid at this point so check for data now
    return channel_poll(m_ts_chans.async);
}

bool ListenSocket::poll()
{
    switch(m_state)
    {
        case CLOSED:
            return false;
        case LISTEN:
        {
            ChanPair chans;
            bool conn_available = false;
            chans = setAccepting(&conn_available);
            if(!conn_available)
                chans = pollListenChans(&conn_available);

            return conn_available;
        }
        case DRAINING:
        case OPEN:
        default:
            assert(false);
    }

    assert(false);
    return false;
}

Status Socket::stat()
{
    return FOS_MAILBOX_STATUS_UNIMPLEMENTED_ERROR;
}

ssize_t FlowSocket::read(void *buf, size_t count, bool blocking)
{
    if (m_state == CLOSED)
    {
        errno = EBADF;
        return -1;
    }

    // see if the socket has been closed while there is still data in the
    // internal buffer to be drained before reading 0
    if(m_state == DRAINING)
    {
        if(m_nbuffered == 0)
        {
            return 0;
        }
        else
            return copyFromBuffer(buf, count);
    }

    assert(m_state == OPEN);

    while(1)
    {
        // FIXME: Where did the 64K number come from? (HK)
        if (m_nbuffered >= count || BUFFER_SIZE - m_nbuffered < 64*1024) 
        {
            return copyFromBuffer(buf, count);
        }

        transport::ReceivedDataEvent *evt;
        size_t size;

        if(blocking && m_nbuffered == 0)
        {
            size = channel_receive_blocking(m_ts_chans.async, (void **)&evt);
        }
        else
        {
            size = channel_poll(m_ts_chans.async);

            if(size == 0)
                return m_nbuffered > 0 ? copyFromBuffer(buf, count) : -EWOULDBLOCK;
            else
                size = channel_receive(m_ts_chans.async, (void **)&evt);
        }

        if(evt->len == 0)
        {
            channel_free(m_ts_chans.async, evt);
            if(m_nbuffered == 0)
            {
                m_state = DRAINING;
                return 0;
            }
            else // we encountered the close of the connection but must provide data until a read() can return a zero.
            {
                // we need to go into a draining state here and continue to 
                // drain the internal buffer until empty, then return 0 on read.
                m_state = DRAINING;
                return copyFromBuffer(buf, count);
            }
        }
        else
        {
            assert(m_nbuffered + evt->len < sizeof(m_buffer));
            memcpy(m_buffer + m_nbuffered, evt->data, evt->len);
            channel_free(m_ts_chans.async, evt);
            m_nbuffered += evt->len;
        }
    }
    return FOS_MAILBOX_STATUS_OK;
}

std::vector<unsigned long long> write_times_start;
std::vector<unsigned long long> write_times_end;

void Socket::dumpStats()
{
    if(write_times_start.size() < 10)
        return;

    for(unsigned int i = 0; i < write_times_start.size(); i++)
        fprintf(stderr, "[%3d] %lld %lld\n",
                i,
                write_times_start[i],
                write_times_end[i]);
}

ssize_t FlowSocket::write(const void *buf, size_t count)
{
    USE_PROF_NETSTACK(unsigned long long start = rdtscll();)

    // block write the data
    ssize_t ret = 0;
    unsigned int written = 0;

    uint8_t *data_p = (uint8_t *)buf;

    USE_TRACE_NETSTACK(if(trace_check((char *)buf)) write_times_start.push_back(rdtscll()));

    while(written < count)
    {
        // FIXME: Why was this lock here? Do we still need it? (HK)
        // ScopedTicketLock sl(m_ticket_lock);
        int amt_to_write = min(count - written, TRANSPORT_CHUNK_SIZE);

        transport::WriteRequest *req = (transport::WriteRequest *)dispatchAllocate(m_ts_chans.sync, sizeof(transport::WriteRequest) + amt_to_write);
        assert(req);
        req->flow_id = m_flow_id;
        req->len = amt_to_write;

        memcpy(req->data, data_p + written, amt_to_write);

        DispatchTransaction trans = dispatchRequest(m_ts_chans.sync, transport::WRITE, req, sizeof(transport::WriteRequest) + amt_to_write);

        transport::WriteReply *reply;
        size_t size = dispatchReceive(trans, (void **)&reply);

        assert(size == sizeof(transport::WriteReply));

        ret = reply->ret;
        dispatchFree(trans, reply);

        if(ret > 0)
            written += ret;
        else
            break;
    }

#ifdef CONFIG_PROF_NETSTACK
    unsigned long long end = rdtscll();
    if(((end - start) / CONFIG_HARDWARE_KHZ) > 10)
        fprintf(stderr, "took > 10 ms.\n");
#endif

    USE_TRACE_NETSTACK(if(trace_check((char *)buf)) write_times_end.push_back(rdtscll()));

    return written ? written : ret;
}

ssize_t FlowSocket::copyFromBuffer(void *buf, size_t count)
{
    if (count >= m_nbuffered)
    {
        memcpy(buf, m_buffer, m_nbuffered);
        ssize_t ret = m_nbuffered;
        m_nbuffered = 0;
        return ret;
    } else { // m_nbuffered > count:
        memcpy(buf, m_buffer, count);
        memmove(m_buffer, m_buffer + count, m_nbuffered - count);
        m_nbuffered -= count;
        return count;
    }
    return FOS_MAILBOX_STATUS_OK;
}

Status Socket::getPeer(struct sockaddr *out_addr)
{
    // FIXME: Not implemented yet
    return FOS_MAILBOX_STATUS_GENERAL_ERROR;
#if 0
    FosStatus ret = FOS_MAILBOX_STATUS_OK;

    // Begin rpc boilerplate code
    GetPeerRequest req = {
        DispatchMessageHeader(),
        *rpcGetResponseMailbox(),
        m_flow_id,
    };

    DispatchToken token = dispatchSendRequest(&m_netstack.alias(), m_netstack.capability(),
            &req, sizeof(req), GET_PEER);

    DispatchResponse *rc = rpcWaitForResponse(token);

    GetPeerReply *reply = (GetPeerReply*)rc->data;

    ret = reply->status;
    *out_addr = reply->addr;
    dispatchResponseFree(rc);
    // END rpc boilerplate code

    return ret;
#endif
}

/* Helper functions */

ChanPair ListenSocket::pollListenChans(bool *conn_available)
{
    ListenChanMap::iterator it;
    ChanPair chans = {NULL, NULL};
    for(it = m_listen_chans.begin(); it != m_listen_chans.end(); it++)
    {
        chans = it->second;
        if(channel_poll(chans.async) > 0)
        {
            *conn_available = true;
            return chans;
        }
    }

    chans.async = endpoint_accept(getAcceptEndpoint());

    if(chans.async)
    {
        Address transport = channel_get_remote_address(chans.async);
        chans.sync = dispatchOpen(transport);
        m_listen_chans.insert(std::make_pair<Address, ChanPair>((Address)transport, (ChanPair)chans));
        *conn_available = true;
    }
    else
        *conn_available = false;

    return chans;
}

// APP -> CM
//
// Called by application from socket code to notify the CM that we are
// actively waiting for a connection. Only generates messages if a
// certain timeout has been exceeded, to avoid DDOS'ing the CM from
// all apps waiting.
ChanPair ListenSocket::setAccepting(bool *conn_available)
{
    *conn_available = false;
    ChanPair chans = {NULL, NULL};
    uint64_t now = rdtscll();
    uint64_t since = now - m_last_conn_update;

    if (ts_to_ms(since) >= conn::LIB_TIMEOUT)
    {
        m_last_conn_update = now;

        // Add ourselves to list in connection manager
        conn::SetAcceptingRequest *req = (conn::SetAcceptingRequest *)dispatchAllocate(m_cm_chan, sizeof(*req));
        req->listen_id = m_flow_id;
        req->app = getAcceptAddress();

        DispatchTransaction token = dispatchRequest(m_cm_chan, conn::ACCEPT, req, sizeof(*req));

        conn::SetAcceptingReply *response;
        size_t size = dispatchReceive(token, (void **) &response);
        assert(size == sizeof(conn::SetAcceptingReply));

        *conn_available = response->connection_available;
        Address netstack = response->netstack;
        dispatchFree(token, response);

        if (*conn_available)
        {
            ListenChanMap::iterator it = m_listen_chans.find(netstack);
            if(it == m_listen_chans.end())
            {
                // Set up the asynchronous listen channel
                while(!chans.async)
                    chans.async = endpoint_accept(getAcceptEndpoint());

                Address remote_addr = channel_get_remote_address(chans.async);

                m_listen_chans.insert(std::make_pair<Address, ChanPair>((Address)remote_addr, (ChanPair)chans));

                chans.sync = dispatchOpen(netstack);
            }
            else
            {
                chans = it->second;
            }

            // FIXME: Given how the TS currently works,this can hang the application
            // incorrectly. This happens when the CM believes that the TS has a
            // pending connection and notifies the application accordingly, but the
            // TS has already given away the connection to some other acceptor (the
            // "oops_valid_app" case. I am leaving this in as I think the coorect
            // way to fix this is to change the way TS works
        }
    }

    return chans;
}

// APP -> TRANS
//
// A connection is available for this application from the transport
// (three-way handshake has been initiated). The application now
// completes the handshake, or fails and re-notifies the CM that we are active.
bool ListenSocket::acceptConnection(FlowSocket * & connection_socket, const conn::ConnectionAcceptedEventApp * event, Channel *chan)
{
    // g_flow_lock.lock();

    // Only make connection socket once -- see Socket::accept()
    if (!connection_socket)
        connection_socket = new FlowSocket(m_domain, m_type, m_protocol);

    // Try to claim connection
    conn::ConnectionAcceptedReply *reply = (conn::ConnectionAcceptedReply *)dispatchAllocate(chan, sizeof(conn::ConnectionAcceptedReply));
    reply->conn_sock_addr = FlowSocket::getFlowAddress();
    reply->flow_id = event->flow_id;

    DispatchTransaction trans = dispatchRequest(chan, transport::CLAIM_CONNECTION, reply, sizeof(conn::ConnectionAcceptedReply));

    // Confirm
    conn::ConnectionAcceptedAck *response;
    size_t size = dispatchReceive(trans, (void **) &response);

    Status status = response->status;
    Address ts_addr = response->ts_addr;
    dispatchFree(trans, response);

    if (status != FOS_MAILBOX_STATUS_OK)
    {
        // We failed on this three-way handshake, so we've been
        // removed from the valid acceptor list for this socket

        // Reset timer so that next entry to setAccepting() will
        // trigger an update to CM.
        m_last_conn_update = 0;

        // g_flow_lock.unlock();

        return false;
    }
    else
    {
        connection_socket->m_flow_id = event->flow_id;

        while((connection_socket->m_ts_chans.async = endpoint_accept(FlowSocket::getFlowEndpoint())) == NULL)
            ;

        while((connection_socket->m_ts_chans.sync = endpoint_accept(FlowSocket::getFlowEndpoint())) == NULL)
            ;

        dispatchListenChannel(connection_socket->m_ts_chans.sync);

        connection_socket->m_state = Socket::OPEN;

        // g_flow_lock.unlock();

        return true;
    }
}

}}}
