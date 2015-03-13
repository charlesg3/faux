#pragma once
#include <sys/socket.h>

#include <name/name.h>

#include <lwip/tcp.h>
#include <lwip/err.h>

//#include <threading/ticket_lock.hpp>

#include <config/config.h>
#include <system/status.hpp>
#include "transport_socket.hpp"
#include "../net.hpp"

#define TRANSPORT_CHUNK_SIZE CONFIG_TRANSPORT_CHUNK_SIZE

#define NET_DEBUG(...) do { USE_DEBUG_NETSTACK(PS(__VA_ARGS__);) } while (0)

// Helper shit
namespace {

inline in_addr_t *ip_to_in(struct ip_addr *addr)
{
    return (in_addr_t *)addr;
}

inline in_addr_t ip_to_in(struct ip_addr addr)
{
    return *ip_to_in(&addr);
}

}

// Transport class
namespace fos { namespace net {
namespace transport {

    // RPC routines (synchronous)

    struct ConnectRequest
    {
        Address client_addr;
        struct in_addr addr;
        uint16_t port;
    };

    struct ConnectReply
    {
        FosStatus status;
        flow_t connection_id;
    };

    struct RecvFromRequest
    {
        // DispatchMessageHeader hdr;
        // messaging::Remotebox rbox;
        // size_t port;
    };

    struct RecvFromReply
    {
        // DispatchMessageHeader hdr;
        // FosStatus status;
        // size_t data_len;
        // size_t from_len;
        // size_t data[0];
    };

    struct LazyUdpInitRequest
    {
        // DispatchMessageHeader hdr;
        // messaging::Remotebox rbox;
    };

    struct LazyUdpInitReply
    {
        // DispatchMessageHeader hdr;
        // FosStatus status;
        // flow_t connection_id;
    };

    struct SendToRequest
    {
        // messaging::Remotebox rbox;
        // flow_t connection_id;
        // int flags;
        // struct in_addr addr;
        // uint16_t port;
        // size_t _size;
    };

    struct SendToReply
    {
        // DispatchMessageHeader hdr;
        // FosStatus status;
    };

    struct WriteRequest
    {
        // messaging::Remotebox rbox;
        flow_t flow_id;
        size_t len;
        char data[0];
    };

    struct WriteReply
    {
        // DispatchMessageHeader hdr;
        ssize_t ret;
    };

    struct GetPeerRequest
    {
        // DispatchMessageHeader hdr;
        // messaging::Remotebox rbox;
        // flow_t connection_id;
    };

    struct GetPeerReply
    {
        // DispatchMessageHeader hdr;
        // FosStatus status;
        // struct sockaddr addr;
    };

    struct CloseRequest
    {
        flow_t flow_id;
    };

    struct CloseReply
    {
        FosStatus status;
    };
    
    struct SetIPRequest
    {
        in_addr_t ip_address;
        in_addr_t netmask;
        in_addr_t gw;
    };

    struct SetIPReply
    {
        FosStatus status;
    };

    struct IncomingRequest
    {
        size_t len;
        uint8_t data[0];
    };

    struct IncomingReply
    {
        FosStatus status;
    };

    struct GetHostByNameRequest
    {
        // DispatchMessageHeader hdr;
        // messaging::Remotebox rbox;
        // size_t len;
        // char hostname;

        // inline size_t size() { return sizeof(GetHostByNameRequest) - 1 + len; }
        // static GetHostByNameRequest * allocate(int len) { GetHostByNameRequest * e = (GetHostByNameRequest *) new char [ sizeof(GetHostByNameRequest) - 1 + len ]; e->len = len; return e; }
    };

    struct GetHostByNameReply
    {
        // DispatchMessageHeader hdr;
        // FosStatus status;
        // struct in_addr addr;
    };

    struct SetIpRequest
    {
        // DispatchMessageHeader hdr;
        // messaging::Remotebox rbox;
        // in_addr_t ip_address;
        // in_addr_t sn_mask;
        // in_addr_t gw_addr;
    };

    struct SetIpReply
    {
        // DispatchMessageHeader hdr;
        // FosStatus status;
    };

    // Events (asynchronous)

    struct ReceivedDataEvent
    {
        size_t len;
        char data[0];
    } __attribute__((packed));

    struct ReceivedMessageEvent
    {
        // size_t len;
        // struct in_addr addr;
        // uint16_t port;
        // char data[0];
    } __attribute__((packed));
    // Dispatch handlers and types
    enum MessageTypes
    {
        NET_SHUTDOWN = TS_MSG_TYPE_START,
        CLAIM_CONNECTION,
        WRITE,
        CLOSE,
        CONNECT,
        GET_HOST_BY_NAME,
        SEND_TO,
        RECV_FROM,
        GET_PEER,
        SET_IP,
        INCOMING,
        NUM_TYPES
    };


    // SEE transport_server.cpp for types and handlers
    // these must be kept in sync with the message types
    // above. --cg3

}}} // namespaces

void fosTransportDumpStats();
bool trace_check(const char *data);

