//#include <rpc/rpc.h>
#include <assert.h>

#include "transport_private.hpp"
#include "transport_server.hpp"
#include "conn_private.hpp"
#include "conn_listen.hpp"
#include "conn_socket.hpp"
#include "transport_callbacks.h"

#include <utilities/tsc.h>
#include <utilities/console_colors.h>
#include <config/config.h>
#include "../common/include/hash.h"

//#include <threading/sched.h>

#ifdef CONFIG_PROF_NETSTACK
extern uint64_t g_tcp_sends;
extern uint64_t g_tcp_bytes_sent;
extern uint64_t g_tcp_cycles_spent;
extern uint64_t g_tcp_fails;
extern uint64_t g_link_send_cycles;
extern uint64_t g_connects;
extern uint64_t g_accepts;
#endif

namespace fos { namespace net { namespace transport {

    template <class T, class T2> T* make_offset(T *a, T2 *b)
    {
        return (T*)((uint8_t *)a - (uint8_t *)b);
    }

    struct WaitThreadArgs
    {
        flow_t flow_id;
        Transport *transport;
    };

    void acceptorWaitThread(void *p)
    {
        WaitThreadArgs *args = (WaitThreadArgs *)p;
#if 0
        dispatchSleep(TRANSPORT_TIMEOUT);

        FlowSocket *out_flow;
        bool is_claimed = args->transport->isClaimed(args->flow_id, &out_flow);

        if (out_flow && is_claimed)
        {
            // Success - we are done
            NET_DEBUG(CONSOLE_COLOR_CYAN "TransportServer: " CONSOLE_COLOR_NORMAL
                    " new connection " CONSOLE_COLOR_YELLOW "[%x]" CONSOLE_COLOR_NORMAL " claimed." CONSOLE_COLOR_NORMAL,
                    args->flow_id);
        }
        else if(out_flow) // flow not claimed
        {
            PS("flow not claimed in time. throwing hands in air.");
            assert(false);
        }
        else // out_flow must be gone because app closed it -- assume it was claimed --cg3
        {
            //out flow gone, assuming app closed.
        }

#endif

        free(p);
    }

    // **** Server callbacks and message handlers

    // Called by lwip when a new connection is accepted.
    err_t acceptCallback(Transport * transport, PendingFlowSocket *pending_flow, struct tcp_pcb * pcb, err_t err)
    {
        USE_PROF_NETSTACK(g_accepts++);
        tcp_finished(pcb, ::connectionFinished);
        NET_DEBUG(CONSOLE_COLOR_BLUE "ConnectionManager: " CONSOLE_COLOR_NORMAL
                CONSOLE_COLOR_RED " << acceptCallback - Start"
                CONSOLE_COLOR_NORMAL " remote port: "
                CONSOLE_COLOR_RED "[%d]"
                CONSOLE_COLOR_NORMAL " local port: "
                CONSOLE_COLOR_RED "[%d]"
                CONSOLE_COLOR_NORMAL, pcb->remote_port, pcb->local_port);

        // FIXME: I don't think this is necessary anymore; letting it be here
        // for now until we verify that this is the case (HK)
        // We *possibly* don't handle chain's of pbufs, so make sure this is one chunk
        assert(pcb->tcp_input_args->inseg.p->next == NULL);

        flow_t flow_id = transport->m_flows.compute(pcb);

        assert(pending_flow->m_flow_id == flow_id);
        assert(pending_flow->m_remote_ip == ip_to_in(pcb->remote_ip));

        if(pending_flow->m_acceptor_valid && pending_flow->m_acceptor_addr == 0)
        {
            transport->echoAccept(pcb);
            return ERR_OK;
        }

        FlowSocket *flow = new FlowSocket();
        flow->m_remote_ip = pending_flow->m_remote_ip;
        flow->m_remote_port = pending_flow->m_remote_port;
        flow->m_client = {NULL, NULL};
        flow->setConnectionId(flow_id);

        // Finish setting up connection
        tcp_recv(pcb, ::connectionRecv);
        tcp_sent(pcb, ::connectionSent);
        tcp_err(pcb, ::connectionError);
        tcp_finished(pcb, ::connectionFinished);
        tcp_arg(pcb, flow);

        transport->m_flows.add(pcb);

        // Hand over connection to acceptor if acceptor exists
        if(pending_flow->m_acceptor_valid)
        {
            // This is ugly, since we use dispatchOpen() to open a unique
            // listen channel (dispatch makes sure it is unique) to the
            // application, but then use vanilla channel_send() to send the
            // conneciton notification on it; this channel is subsequently used
            // by the application to claim the connection (via a
            // dispatchRequest)
            flow->m_client_addr = pending_flow->m_acceptor_addr;
            Channel *chan;
            std::map<Address, Channel *>::iterator it = transport->m_conn_handoff_chans.find(flow->m_client_addr);
            if(it == transport->m_conn_handoff_chans.end())
            {
                // Open asynchronous (non-dispatch) connection handoff channel
                chan = endpoint_connect(transport->getEndpoint(), flow->m_client_addr);
                transport->m_conn_handoff_chans.insert(
                        std::make_pair<Address, Channel *>(flow->m_client_addr, chan));

                // The synchronuos (dispatch) channel is opened by the client
                // via dispatchOpen()
            }
            else
            {
                chan = it->second;
            }

            // Notify acceptor of connection
            conn::ConnectionAcceptedEventApp *app_data = 
                (conn::ConnectionAcceptedEventApp *) channel_allocate(chan, sizeof(*app_data));
            app_data->addr = ip_to_in(pcb->remote_ip);
            app_data->port = flow->m_remote_port;
            app_data->flow_id = flow_id;

            channel_send(chan, app_data, sizeof(*app_data));

            // FIXME: Don't handle timeout right now
            // transport->handleTimeout(flow_id, pending_flow->m_acceptor_rbox);
        }

        // Free memory
        delete pending_flow;

        return ERR_OK;
    }

}}}
