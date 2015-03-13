#include <assert.h>
#include <stdint.h>

#include <arpa/inet.h>

#include <config/config.h>

#include <sys/net/types.h>
#include <sys/net/transport.h>

#include "transport_server.hpp"
#include "transport_callbacks.h"
#include "transport_marshall.hpp"

#include "conn_manager.hpp"
#include "conn_socket.hpp"

#include "../common/include/hash.h"

#include "lwip/ip_addr.h"
#include "lwip/netif.h"

using namespace fos;
using namespace net;
using namespace transport;

void dumpSocketStats();

#define min(a,b) (((a) < (b)) ? (a) : (b))

#ifdef CONFIG_PROF_NETSTACK
uint64_t g_tcp_sends = 0;
uint64_t g_tcp_bytes_sent = 0;
uint64_t g_tcp_cycles_spent = 0;
uint64_t g_tcp_fails = 0;
uint64_t g_link_send_cycles = 0;
uint64_t g_connects = 0;
uint64_t g_accepts = 0;
uint64_t g_claims = 0;
#endif

Transport *Transport::g_transport = NULL;

// FIXME: Ugly hack
extern void idle(void *ignored);

Transport::Transport(struct netif *nif, int id, int num_transports)
    : m_netif(nif)
    , m_num_transports(num_transports)
    , m_id(id)
    , m_ip_addr(0)
{
    assert(g_transport == NULL);
    g_transport = this;

    m_conn = new conn::ConnManagerTransportLink(this, id, num_transports);

    m_endpoint = endpoint_create();
    m_address = endpoint_get_address(m_endpoint);
    name_register(TRANSPORT_NAME, m_address);

    if(m_id != 0)
        name_register(TRANSPORT_NAME + id, m_address);
    else
    {
        m_child_transports = (Channel **)malloc(sizeof(Channel *) * (m_num_transports - 1));
        for(unsigned int i = 0; i < m_num_transports - 1 /* don't need our own */ ; i++)
        {
            Address child_addr;
            while(!name_lookup(TRANSPORT_NAME + i + 1, &child_addr))
                ;
            m_child_transports[i] = dispatchOpen(child_addr);
        }
    }

    dispatchListen(m_endpoint);

    dispatchRegister(WRITE, transport::write);
    dispatchRegister(CLOSE, transport::close);
    dispatchRegister(CLAIM_CONNECTION, claimConnection);
    dispatchRegister(SET_IP, transport::setIP);
    dispatchRegister(INCOMING, transport::incoming);
    dispatchRegister(CONNECT, transport::connect);
    dispatchRegister(conn::PROVIDE_APP_WITH_CONNECTION, provideAppWithConnectionCallback);
    dispatchRegisterIdle(idle, NULL); // idle() is in netstack.cpp; should move somewhere else

    tcp_set_min_local_port(2048 * (m_id + 1));
    tcp_set_max_local_port(2048 * (m_id + 2));
}

void Transport::go()
{
    dispatchStart();
}


extern "C" void netif_rx(unsigned char* data, int len);

// This function is called when the link layer delivers data to the transport layer.
// It is responsible for pushing the data up through the higher layers of processing.
//
// Those higher layers make callbacks e.g. when data is received (see recvCallback)
void Transport::incoming(void * vpdata, size_t len)
{
    netif_rx((unsigned char *)vpdata, len);
}

void Transport::incomingBroadcast(void * vpdata, size_t len)
{
    assert(m_id == 0);  //Make sure that only the master transport is broadcasting

    for(unsigned int i = 0; i < m_num_transports - 1 /* just set our own */; i++)
    {
        DispatchTransaction trans;
        transport::IncomingReply *reply;
        transport::IncomingRequest *req = (transport::IncomingRequest *) dispatchAllocate(m_child_transports[i],
            sizeof(transport::IncomingRequest) + len);
        __attribute__((unused)) size_t size;

        assert(req);

        req->len = len;
        memcpy(req->data, vpdata, len);

        trans = dispatchRequest(m_child_transports[i], transport::INCOMING, req, sizeof(transport::IncomingRequest) + len);

        size = dispatchReceive(trans, (void **) &reply);
        assert(size == sizeof(transport::IncomingReply));
        assert(reply->status == FOS_MAILBOX_STATUS_OK);
        dispatchFree(trans, reply);
    }
}

// This callback occurs when we receive an ip address from the DHCP server
// We need to forward the ip address to the other netstacks
void Transport::ifUpCallback(in_addr_t ip_addr, in_addr_t netmask, in_addr_t gw)
{
    assert(m_id == 0);  //Make sure that only the master transport is getting an ip addr

    setIP(ip_addr, netmask, gw); // Set our own ip address

    // Send the ip address to the child netstacks
    for(unsigned int i = 0; i < m_num_transports - 1 /* just set our own */; i++)
    {
        DispatchTransaction trans;
        transport::SetIPReply *reply;
        transport::SetIPRequest *req = (transport::SetIPRequest *) dispatchAllocate(m_child_transports[i],
            sizeof(transport::SetIPRequest));
        __attribute__((unused)) size_t size;

        assert(req);

        req->ip_address = m_ip_addr;
        req->netmask = netmask;
        req->gw = gw;

        trans = dispatchRequest(m_child_transports[i], transport::SET_IP, req, sizeof(transport::SetIPRequest));

        size = dispatchReceive(trans, (void **) &reply);
        assert(size == sizeof(transport::SetIPReply));
        assert(reply->status == FOS_MAILBOX_STATUS_OK);
        dispatchFree(trans, reply);
    }
}


FosStatus Transport::setIP(in_addr_t ip_addr, in_addr_t netmask, in_addr_t gw)
{
    if(m_id == m_num_transports - 1)
    {
        char ipaddr_s[64];
        inet_ntop(AF_INET, &ip_addr, ipaddr_s, 64);
        fprintf(stderr, " \033[22;32m*\033[0m       ip address: %s\n", ipaddr_s);
    }
    m_ip_addr = ip_addr;
    netif_set_ipaddr(m_netif, (struct ip_addr *)&ip_addr);
    netif_set_netmask(m_netif, (struct ip_addr *)&netmask);
    netif_set_gw(m_netif, (struct ip_addr *)&gw);
    netif_set_up(m_netif);
    return FOS_MAILBOX_STATUS_OK; 
}

ssize_t Transport::write(flow_t connection_id, const void * data, size_t len)
{
    USE_TRACE_NETSTACK(if(trace_check((char *)data)) twrite_times_start.push_back(rdtscll()));

    uint8_t *data_p = (uint8_t *)data;

    USE_PROF_NETSTACK(unsigned long long start = rdtscll();)
    FlowSocket * flow = getFlowSocket(connection_id);
    if (!flow) return -1;

//    ScopedTicketLock sl(flow->m_ticket_lock);

    struct tcp_pcb *pcb = m_flows.get(connection_id);
    assert(pcb);
    assert(len <= TRANSPORT_CHUNK_SIZE);

    int rc = tcp_write(pcb, data, len, TCP_WRITE_FLAG_COPY);

    USE_TRACE_NETSTACK(if(trace_check((char *)data_p)) twrite_times_enq.push_back(rdtscll()));

    if(rc == 0)
    {
        USE_PROF_NETSTACK(unsigned long long send_start = rdtscll();)
        tcp_output(pcb);
        USE_PROF_NETSTACK(g_link_send_cycles += rdtscll() - send_start;)
    }

#ifdef CONFIG_PROF_NETSTACK
    if(rc == 0)
    {
        g_tcp_sends++;
        g_tcp_bytes_sent += len;
        g_tcp_cycles_spent += rdtscll() - start;
    }
    else
        g_tcp_fails++;


    if(((rdtscll() - start) / CONFIG_HARDWARE_KHZ) > 10)
        fprintf(stderr, "took > 10 ms.\n");
#endif

    USE_TRACE_NETSTACK(if(trace_check((char *)data)) twrite_times_end.push_back(rdtscll()));

    return rc == 0 ? len : -1;
}


FosStatus Transport::close(flow_t connection_id)
{
    FlowSocket * flow = getFlowSocket(connection_id);
    if (!flow) return FOS_MAILBOX_STATUS_BAD_PARAMETER_ERROR;
    if(flow->m_state == FlowSocket::CLOSE_WAIT)
        flow->m_state = FlowSocket::LAST_ACK;
    else
        flow->m_state = FlowSocket::TIME_WAIT;
    closeConnection(flow);
    return FOS_MAILBOX_STATUS_OK;
}

void Transport::closeConnection(FlowSocket * flow)
{
    assert(flow);

    switch(flow->type())
    {
        case CONNECT_SOCKET:
        case FLOW_SOCKET:
        {
            flow_t hash = flow->m_flow_id;
            struct tcp_pcb * pcb = m_flows.get(hash);

            //be sure this is still an active conection
            if (pcb == NULL) break;

            NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
            "removing flow hash: %x rp: " CONSOLE_COLOR_RED "[%d]" CONSOLE_COLOR_NORMAL
            " lp: " CONSOLE_COLOR_RED "[%d]" CONSOLE_COLOR_NORMAL,
            hash, pcb->remote_port, pcb->local_port);

            // close the connection
            tcp_sent(pcb, NULL);
            tcp_recv(pcb, NULL);

            if (flow->m_state == FlowSocket::CLOSE_WAIT)
            {
                ReceivedDataEvent *close_event = (ReceivedDataEvent *)channel_allocate(flow->m_client.async, sizeof(ReceivedDataEvent));
                close_event->len = 0;
                channel_send(flow->m_client.async, close_event, sizeof(ReceivedDataEvent));
            }
            else
            {
                dispatchIgnoreChannel(flow->m_client.sync);
                // FIXME: Closing the channel here still causes the app to crash when
                // it calls dispatchCloseChannel()
                // channel_close(flow->m_client.sync);
                // channel_close(flow->m_client.async);
                m_flows.remove(hash);

                if(flow->m_state == FlowSocket::TIME_WAIT)
                    tcp_close(pcb);

                delete flow;
            }

            break;
        }
        case MESSAGE_SOCKET:
        {
            conn::MessageSocket *msocket = (conn::MessageSocket *)flow;
            flow_t hash = msocket->getConnectionId();

            struct udp_pcb * pcb = (struct udp_pcb *)m_flows.get(hash);

            NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
            "removing flow with hash: %x for remote port: " CONSOLE_COLOR_RED "[%d]" CONSOLE_COLOR_NORMAL, hash, pcb->remote_port);

            assert(pcb);

            m_flows.remove(hash);
            break;
        }
        /*
        case FLOW_SOCKET:
        {
            printf("closing flow socket...?\n");
            break;
        }
        */
        default:
            PS("don't know how to close this socket type: %d", flow->type());
            assert(false);
    }
}

#if 0

void Transport::recvMessageCallback(FlowSocket * flow, struct udp_pcb * pcb, struct pbuf * p, struct in_addr *addr, uint16_t port)
{
    ReceivedMessageEvent event;
    event.len = p->tot_len;

    // count the pbufs
    struct pbuf *pb;
    int pb_cnt = 0;
    for(pb = p; pb != NULL; pb = pb->next, ++pb_cnt);

    struct iovec iov[pb_cnt + 1];
    iov[0].iov_base = &event;
    iov[0].iov_len = sizeof(event);

    // create the iov
    int i;
    for(pb = p, i = 0; pb != NULL; pb = pb->next, i++)
    {
        iov[i + 1].iov_base = pb->payload;
        iov[i + 1].iov_len = pb->len;
    }

    bool blocking = false;
    Status rc = flow->m_client.sendV(iov, pb_cnt + 1, blocking);

    // Place back pressure on sender if we can't deliver
    if(rc == FOS_MAILBOX_STATUS_NO_SPACE_ERROR)
    {
        printf("dropping received message\n"); //FIXME: do something better?
    }

    pbuf_free(p);
}

#endif

err_t Transport::recvCallback(FlowSocket * flow, struct tcp_pcb * pcb, struct pbuf * p, err_t err)
{
    // If no application has claimed this connection yet, then we want
    // lwip to buffer data and slow down the sender if we are running
    // out of buffer space
    //
    // If we have the pending delivery flag set then we are already trying to
    // send data to the client (and have been swapped out) and thus cannot
    // start trying to send an additional packet
    //if (!flow->m_claimed || flow->m_pending_delivery)
    if (!flow->m_claimed)
    {
        return ERR_BUF;
    }

    if (err != ERR_OK)
    {
        assert(false);          // wtf is this? -nzb
        pbuf_free(p);
    }

    if (p == NULL)
    {
        flow->m_state = FlowSocket::CLOSE_WAIT;
        closeConnection(flow);
    }
    else
    {
        flow->m_pending_delivery = true;
//        ScopedTicketLock sl(flow->m_recv_ticket_lock);

        // forward data to app
        ReceivedDataEvent *event = (ReceivedDataEvent *)channel_allocate(flow->m_client.async, sizeof(ReceivedDataEvent) + p->tot_len);
        event->len = p->tot_len;

        char *data_p = event->data;

        // count the pbufs
        int pb_cnt = 0;
        struct pbuf *pb;
        for(pb = p; pb != NULL; pb = pb->next, ++pb_cnt)
        {
            memcpy(data_p, pb->payload, pb->len);
            data_p += pb->len;
        }

        int size = p->tot_len;

        channel_send(flow->m_client.async, event, sizeof(ReceivedDataEvent) + p->tot_len);

        // Acknowledge to the stack that we received the data
        tcp_recved(pcb, size);

        pbuf_free(p);
        flow->m_pending_delivery = false;
    }

    return ERR_OK;
}

void Transport::connect(Channel* chan, DispatchTransaction trans, Address client_addr, struct in_addr addr, uint16_t port)
{
    struct tcp_pcb *pcb = (struct tcp_pcb *)tcp_new();
    if (!pcb)
    {
        assert(false); // @todo Properly handle this error case
        return;
    }

    //PS("making connect call.");

    port = ntohs(port);
    if (tcp_connect(pcb, (ip_addr*)&addr, port, connectionConnected) != ERR_OK)
    {
        assert(false); // @todo Properly handle this error case
        tcp_close(pcb);
    }
    else
    {
        // Success -- fill in callback info for future callbacks
        // @todo make sure this gets free'd on close
        transport::flow_t sock_id;
        sock_id = JSNetHash(addr.s_addr, port, 0, 0);
        ConnectSocket * connect_socket = new ConnectSocket();
        connect_socket->m_chan = chan;
        connect_socket->m_trans = trans;
        connect_socket->setConnectionId(sock_id);
        connect_socket->m_claimed = true;
        connect_socket->m_client_addr = client_addr;
        tcp_arg(pcb, connect_socket);

        // this may cause a race condition, however the time to send
        // the syn/syn_ack will be much longer than the time to
        // register the flow.

        // because we now drop packets unless a valid mapping exists,
        // I do not believe there is a race condition any longer -nzb
        //FIXME: registerFlow(hash); //not done in faux??
    }
}

err_t Transport::connectCallback(ConnectSocket * flow, struct tcp_pcb *tpcb, err_t err)
{
    USE_PROF_NETSTACK(g_connects++);

    tcp_recv(tpcb, ::connectionRecv);
    tcp_sent(tpcb, ::connectionSent);
    tcp_finished(tpcb, ::connectionFinished);

    // @fixme this is disabled because we are requiring that flow
    // is the first bytes of ConnectData so that delete works
    // correctly later

    // change the callback data to FlowData, because next time the arg
    // is used is in Recv or Error -nzb
    //   tcp_arg(tpcb, &data->flow);

    assert(err == ERR_OK);

    ConnectReply *reply = (ConnectReply *)dispatchAllocate(flow->m_chan, sizeof(ConnectReply));
    reply->status = FOS_MAILBOX_STATUS_OK;
    reply->connection_id = m_flows.add(tpcb);

    dispatchRespond(flow->m_chan, flow->m_trans, reply, sizeof(*reply));

    Endpoint *ep = Transport::transport().getEndpoint();
    flow->m_client.async = endpoint_connect(ep, flow->m_client_addr);

    return ERR_OK;
}
void Transport::finishedCallback(FlowSocket * flow, struct tcp_pcb *tpcb)
{
#if 0
    unregisterFlow(flow->m_connection_id);
    if(m_flows.contains(flow->m_connection_id))
        m_flows.remove(flow->m_connection_id);   //BMK move here
    else
        ; // I guess the app already quit? --cg3
    delete flow;
#endif
}

bool Transport::isClaimed(flow_t connection_id, FlowSocket ** out_flow)
{
    FlowSocket * flow = getFlowSocket(connection_id);

    if (out_flow) *out_flow = flow;

    if (!flow)
    {
        NET_DEBUG(CONSOLE_COLOR_BLUE "ConnectionManager: " CONSOLE_COLOR_NORMAL
                "Flow " CONSOLE_COLOR_RED "[%x]" CONSOLE_COLOR_NORMAL " not found.", connection_id);
        return true;            // not available, so treat it as claimed
    }

    return flow->m_claimed;
}


FlowSocket * Transport::getFlowSocket(flow_t connection_id)
{
    struct tcp_pcb * pcb;
    FlowSocket * flow = 0;

    pcb = m_flows.get(connection_id);
    if (!pcb)
        return NULL;

    flow = cast<FlowSocket>(pcb->callback_arg);
    return flow;
}

void Transport::pushRefusedPackets(flow_t connection_id)
{
    struct tcp_pcb * pcb = m_flows.get(connection_id);
    if (pcb)
        tcp_pushrefused(pcb);
}

// A small container class for pcbs based on net hash of 4-tuple

const flow_t CLIENT_SIDE_BIT = 0x1;
const flow_t CLIENT_SIDE_MASK = ~CLIENT_SIDE_BIT;

flow_t Transport::FlowMap::compute(struct tcp_pcb_listen * pcb)
{
    flow_t result = JSNetHash(ip_to_in(pcb->local_ip), pcb->local_port, 0, 0);

    return result;
}

flow_t Transport::FlowMap::compute(struct tcp_pcb * pcb)
{
    flow_t result = JSNetHash(ip_to_in(pcb->local_ip), pcb->local_port, ip_to_in(pcb->remote_ip), pcb->remote_port);

    return result;
}

flow_t Transport::FlowMap::compute(struct udp_pcb * pcb)
{
    flow_t result = JSNetHash(ip_to_in(pcb->local_ip), pcb->local_port, ip_to_in(pcb->remote_ip), pcb->remote_port);

    return result;
}

flow_t Transport::FlowMap::add(struct tcp_pcb * pcb)
{
    assert(pcb);

    flow_t hash = compute(pcb);

    if(m_flows.find(hash) != m_flows.end())
    {
        unsigned int size = m_flows.size();
        fprintf(stderr, "Clash with [%d] flows.\n", size);
        for(InternalFlowMap::iterator it = m_flows.begin(); it != m_flows.end(); it++)
        {
            fprintf(stderr, "  Hash: %x lp: %d rp: %d l_ip: %x rip: %x\n", it->first, (*it->second).local_port, (*it->second).remote_port, *(unsigned int *)&(*it->second).local_ip, *(unsigned int *)&(*it->second).remote_ip);
        }
        fprintf(stderr, "New hash: %x new lp: %d rp: %d l_ip: %x rip: %x\n", hash, pcb->local_port, pcb->remote_port, *(unsigned int *)&pcb->local_ip, *(unsigned int *)&pcb->remote_ip);
    }

    assert(m_flows.find(hash) == m_flows.end());
    m_flows[ hash ] = pcb;
    //PS("adding hash: %x", hash);
    return hash;
}

flow_t Transport::FlowMap::add(struct udp_pcb * pcb)
{
    assert(pcb);

    flow_t hash = compute(pcb);

    if(m_flows.find(hash) != m_flows.end())
    {
        unsigned int size = m_flows.size();
        fprintf(stderr, "Clash with [%d] flows.\n", size);
        for(InternalFlowMap::iterator it = m_flows.begin(); it != m_flows.end(); it++)
        {
            fprintf(stderr, "  Hash: %x lp: %d rp: %d l_ip: %x rip: %x\n", it->first, (*it->second).local_port, (*it->second).remote_port, *(unsigned int *)&(*it->second).local_ip, *(unsigned int *)&(*it->second).remote_ip);
        }
        fprintf(stderr, "New hash: %x new lp: %d rp: %d l_ip: %x rip: %x\n", hash, pcb->local_port, pcb->remote_port, *(unsigned int *)&pcb->local_ip, *(unsigned int *)&pcb->remote_ip);
    }

    assert(m_flows.find(hash) == m_flows.end());
    PS("adding hash: %x", hash);
    m_flows[ hash ] = (struct tcp_pcb *)pcb;
    return hash;
}

struct tcp_pcb * Transport::FlowMap::get(flow_t id)
{
    InternalFlowMap::iterator it = m_flows.find(id);
    if (it == m_flows.end()) {
        //fprintf(stderr, "--->Get: id=%x, pcb=NONE (error)\n", id);
        return NULL;
    }
    else {
        //fprintf(stderr, "-->Get: found id=%x, pcb=%p\n", id, it->second);
        return it->second;
    }
}

bool Transport::FlowMap::contains(flow_t id)
{
    InternalFlowMap::iterator it = m_flows.find(id);
    return it != m_flows.end();
}

void Transport::FlowMap::remove(flow_t id)
{
    InternalFlowMap::iterator it = m_flows.find(id);
    assert(it != m_flows.end());
    m_flows.erase(it);
}

void Transport::incomingConn(in_addr_t source, uint16_t source_port,
        flow_t listen_sock_id, flow_t flow_id,
        Address acceptor, bool acceptor_valid,
        void *syn, size_t syn_len)
{
    uint8_t * data = (uint8_t *) syn;
    struct ethernet_frame *p = (struct ethernet_frame *)data;

    int type = ntohs(p->type);
    assert(type == 0x0800); // ip

    struct ip_frame *ip = (struct ip_frame *)&data[sizeof(struct ethernet_frame)];

    if ( (ip->version & 0xf0)  != 0x40 ) {
        printf("Bad packet in linkserver bad IP version %x\n", ip->version);
        return; //dump invalid packet
    }
    assert(ip->protocol == 6); //tcp

    unsigned int header_length = ip->version & 0xF;
    struct tcp_frame *t = (struct tcp_frame *)&data[sizeof(struct ethernet_frame) + (header_length * 4)];

    uint8_t flags = ((uint8_t *)&t->control)[1]; // fix the byte order of the control field

    assert((flags & TCP_SYN) && ! (flags & TCP_ACK)); // SYN packet

    // in_addr_t source = ip->source;
    // in_addr_t dest = ip->dest;
    // in_addr_t dest_swapped = ip_to_in((ip_addr *)&dest);
    // in_addr_t dest_swapped = dest;

    struct tcp_input_args args;
    memset(&args, 0, sizeof(args));
    args.packet_len = syn_len;
    args.iphdr = (ip_hdr *)ip;
    args.tcphdr = (tcp_hdr *)t;

    args.tcphdr->src = ntohs(args.tcphdr->src);
    args.tcphdr->dest = ntohs(args.tcphdr->dest);
    args.seqno = args.tcphdr->seqno = ntohl(args.tcphdr->seqno);
    args.ackno = args.tcphdr->ackno = ntohl(args.tcphdr->ackno);
    args.tcphdr->wnd = ntohs(args.tcphdr->wnd);
    args.flags = TCPH_FLAGS(args.tcphdr) & TCP_FLAGS;
    args.tcplen = syn_len + ((args.flags & TCP_FIN || args.flags & TCP_SYN)? 1: 0);

    // FIXME:
    // There's a memory leak here if the remote end goes away without
    // finishing connection establishment
    //
    // This memory is freed in conn:acceptCallback()
    PendingFlowSocket *pending_flow = new PendingFlowSocket();
    pending_flow->m_remote_ip = source;
    pending_flow->m_remote_port = source_port;
    pending_flow->m_flow_id = flow_id;
    pending_flow->m_listen_sock_id = listen_sock_id;
    pending_flow->m_acceptor_valid = acceptor_valid;
    if(acceptor_valid)
        pending_flow->m_acceptor_addr = acceptor;
    // pending_flow->m_cm_rbox = req->cm_rbox;

    tcp_listen_input_closed(&args, 0, pending_flow, ::connectionAccept);
}


void Transport::provideAppWithConnection(
        transport::flow_t &flow_id, 
        Address acceptor,
        bool *is_closed,
        bool *is_claimed,
        Address *oops_valid_app)
{
    // check if the flow has already been claimed, send response to conn mgr
    transport::FlowSocket * flow;

    isClaimed(flow_id, &flow);

    if (!flow)
    {
        // The following seems to assume that the reason we could not find
        // the associated flow was because it had already been claimed
        // (after we thought the application that was supposed to accept had
        // timed out) and the CM had been notified as such, but then the
        // application did claim the flow and has subsequently closed it
        // without the CM being aware of this development at the time it
        // sent us the new acceptor
        *is_closed = true;
        return;
    }

    *is_closed = false;
    *is_claimed = flow->m_claimed;
    if (flow->m_claimed)
    {
        *oops_valid_app = flow->m_client_addr;
    }

    Channel *chan = dispatchOpen(acceptor);

    // This is ugly since I use the dispatch library to create the channel (the
    // dispatch library takes care to not use duplicate channels for the same
    // address), but then use it below to send a message using the bare channel
    // API
    // This might also hit performance in the case when the acceptor is not
    // immediately around to call endpoint_accept(): the TS has to wait until
    // that happens
    conn::ConnectionAcceptedEventApp *app_data = 
        (conn::ConnectionAcceptedEventApp *) channel_allocate(chan, sizeof(*app_data));
    app_data->addr = flow->m_remote_ip;
    app_data->flow_id = flow_id;

    channel_send(chan, app_data, sizeof(*app_data));

    // FIXME: Don't handle timeout yet
    // m_transport->handleTimeout(flow_id, acceptor);

    return;
}

// void Transport::handleTimeout(flow_t flow_id, messaging::Remotebox acceptor)
// {
//     // Shouldn't get here;
//     // To be implemented along the lines of the timeout handling code in 
//     // provideAppWIthConnectionCallback() in the old netstack
//     assert(false);
// }


/************************
 *     echo server
 ***********************/
static int g_echo_port = 0;

static err_t echo_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    if(!p)
    {
        tcp_close(pcb);
        return ERR_ABRT;
    }
    tcp_write(pcb, p->payload, p->tot_len, TCP_WRITE_FLAG_COPY);
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

void echo_finished(void *arg, struct tcp_pcb *tpcb)
{
}

void echo_error(void *arg, err_t err)
{
    if(err == ERR_RST)
    {
//        PS("rst.");
    }
}

void Transport::startEcho(int port)
{
    //register with the conn manager as usual
    transport::flow_t listen_sock_id;
    conn::ConnManager &conn = conn::ConnManager::connManager();
    int addr = 0;
    conn.bindAndListen(*(in_addr *)&addr, htons(port), true, false, &listen_sock_id);
    g_echo_port = port;
}

void Transport::echoAccept(tcp_pcb *pcb)
{
    tcp_recv(pcb, echo_recv);
    tcp_err(pcb, echo_error);
    tcp_finished(pcb, echo_finished);
    tcp_arg(pcb, NULL);
}

int Transport::echoPort()
{
    return g_echo_port;
}
