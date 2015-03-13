#pragma once

#include <messaging/dispatch.h>

namespace fos { namespace net { namespace transport {

    class Transport;
    class PendingFlowSocket;

    /*
       void transportApplicationTimeout(void * vpmsg, size_t size, DispatchToken transport_token, void * vptrans);
       */
    err_t acceptCallback(Transport * transport, PendingFlowSocket *pending_flow, struct tcp_pcb * pcb, err_t err);

    void acceptorWaitThread(void *p);

}}}
