// #include <dispatch/dispatch.h>
#include <messaging/dispatch.h>

namespace fos { namespace net { namespace conn {

    void connectCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void establishOutgoingConnCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void bindCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void bindAndListenCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void peerBindAndListenCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void performBindCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void acceptCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void incomingConn(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void establishIncomingConnCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void provideAppWithConnectionCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void handleTimeoutCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);
    void stealConnCallback(Channel* chan, DispatchTransaction trans, void* msg, size_t size);

}}}
