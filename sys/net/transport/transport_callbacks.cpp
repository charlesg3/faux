#include <assert.h>
#include <sys/net/transport.h>
#include <messaging/dispatch.h>
#include <messaging/channel.h>
#include <utilities/debug.h>
#include "transport_server.hpp"
#include "transport_private.hpp"
#include "transport_marshall.hpp"
#include "transport_callbacks.h"
#include "conn_listen.hpp"

// It makes sense to do this here, since ideally all these functions would be in
// the fos::net::transport namespace; the only reason they are not is that lwip
// needs to use them
using namespace fos::net::transport;

/*
void udpRecv(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr * addr, u16_t port)
{
    FlowSocket * flow = cast<FlowSocket>(arg);
    Transport::transport().recvMessageCallback(flow, pcb, p, (struct in_addr *)addr, (uint16_t)port);
}
*/

err_t connectionRecv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    FlowSocket * flow = cast<FlowSocket>(arg);
    return Transport::transport().recvCallback(flow, pcb, p, err);
}

err_t connectionSent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    return ERR_OK;
}

void connectionError(void *arg, err_t err)
{
    if(err == ERR_RST)
    {
        // FIXME: When a RST is received during a conn/sec benchmark,
        // it invariably causes a crash in closeConnection (below)
        // because flow->m_client.async == NULL. My guess is that the
        // cast of arg from void* --> FlowSocket* is incorrect, and
        // this arg never had an async channel established.
        // 
        // The current workaround is just to punt on RSTs. -nzb
        return;

        if (arg)
        {
            FlowSocket * flow = cast<FlowSocket>(arg);
            if (flow)
            {
                flow->m_state = FlowSocket::CLOSE_WAIT; // FIXME: Is this the right thing to do?
                Transport::transport().closeConnection(flow);
            }
        }
        else
            PS("Reset on connection that's already closed");
    }
    else
        PS("netstack had a connection error: %d\n", err);
}

err_t connectionAccept(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    err_t err_ret;
    err_ret = acceptCallback(&Transport::transport(), (PendingFlowSocket *) arg, tpcb, err);
    return err_ret;
}

err_t connectionConnected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    ConnectSocket * flow = cast<ConnectSocket>(arg);
    return Transport::transport().connectCallback(flow, tpcb, err);
}

void connectionFinished(void *arg, struct tcp_pcb *tpcb)
{
    FlowSocket * flow = cast<FlowSocket>(arg);
    Transport::transport().finishedCallback(flow, tpcb);
}

