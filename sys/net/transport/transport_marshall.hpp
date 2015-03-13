#pragma once

//#include <messaging/messaging.hpp>
#include "./transport_private.hpp"

namespace fos { namespace net {
namespace transport {

#if 0
void idle(void * vptrans);
void incoming(void * data, size_t len, void * vptrans);
void transferData(void * data, size_t len, void * vptrans);
void connect(void * vpmsg, size_t size, DispatchToken token, void * vptrans);
#endif
void bindAndListen(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
void write(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
void close(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
void setIP(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
void incoming(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
void connect(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
void provideAppWithConnectionCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
//void bindAndListen(void * vpmsg, size_t size, DispatchToken token, void * vptrans);
#if 0
void sendTo(void * vpmsg, size_t size, DispatchToken token, void * vptrans);
void recvFrom(void * vpmsg, size_t size, DispatchToken token, void * vptrans);
void getPeer(void * vpmsg, size_t size, DispatchToken token, void * vptrans);
void hostFound(const char * hostname, struct ip_addr * ipaddr, void * vparg);
void getHostByName(void * vpmsg, size_t size, DispatchToken token, void * vptrans);
void setIp(void * vpmsg, size_t size, DispatchToken token, void * vptrans);
void netShutdown(void * vpmsg, size_t size, DispatchToken token, void * vptrans);
#endif

}}}
