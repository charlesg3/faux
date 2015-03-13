#pragma once
#include "transport_private.hpp"
#include "../net.hpp" // this path sucks

namespace fos { namespace net { namespace conn {

// Messages for three-way handshake to assign connection to an
// application via a netstack.
// 
// There are three components involved: connection manager (netstack),
// transport (netstack), and application.
//
// This diagram shows the messages that can be sent, as well as the
// sequence they are sent in. A star designates special cases, whereas
// common behavior for an available connection follows the path 1-5.
//
//                   +-------+
//                   |       |
//   +- 2 CAET ---   |  CM   |   <---1 CSAReq -+
//   |  +- 3*CAER -> |       |    -- 2*CAE -+  |
//   |  |            +-------+              |  |
//   |  |                                   |  |
//   v                                      v
// +-------+                             +-------+
// |       |       ---- 3 CAE ---->      |       |
// | Trans |      <-- 4 CAEReply --      |  App  |
// |       |       --- 5 CAEAck -->      |       |
// +-------+                             +-------+

// Timeouts for various points of handshake
// Message types
enum ConnMessageType
{
    BIND_AND_LISTEN = CM_MSG_TYPE_START, 
    PEER_BIND_AND_LISTEN,
    ACCEPT,
    ESTABLISH_INCOMING_CONN,
    PROVIDE_APP_WITH_CONNECTION,
    SYN,
    // CONNECT,
    // BIND,
    // PEER_BIND_AND_LISTEN,
    // APP_TIMEOUT,
    // ESTABLISH_OUTGOING_CONN,
    // PERFORM_BIND,
    // STEAL_CONN,
};

const unsigned int TRANSPORT_TIMEOUT = 500;
const unsigned int LIB_TIMEOUT = 250;

// Data needed to identify a connection
struct Connection
{
    Channel *netstack;
    transport::flow_t connection_id;
};

struct BindAndListenRequest
{
    uint16_t port;
    struct in_addr addr;
    bool shared;
};

struct BindAndListenReply
{
    FosStatus status;
    transport::flow_t listen_sock_id;
};

// APP -> CM
//
// Send from app to conn mgr when timeout occurs to keep the
// application at the front of the acceptor list
struct SetAcceptingRequest
{
    transport::flow_t listen_id;
    Address app;
};

// CM -> APP
//
// Sent from conn mgr to app in response to a set accepting update, to
// the let the application know whether a new connection is available
// and incoming to the socket's mailbox.
struct SetAcceptingReply
{
    FosStatus status;
    bool connection_available;
    Address netstack;
};

// CM -> TRANS
//
// Sent from the conn mgr to transport when a new acceptor is
// available for a connection that has already been set up.
//
// This is the first stage of the three-way handshake.
struct ConnectionAcceptedEventTransport
{
    Address app;
    transport::flow_t flow_id;
};

// CM -> TRANS
//
// Sent from the conn mgr to transport when a new connection is available. This
// version, which includes info from the SYN packet, is sent when the connection
// is first established so the transport can do proper tcp processing to start
// receiving data. This will replace ConnectionAcceptedEventTransportWithPcb
//
// This is the first stage of the three-way handshake.
struct ConnectionAcceptedEventTransportSyn
{
    transport::flow_t listen_sock_id;
    transport::flow_t flow_id;
    in_addr_t addr;
    uint16_t port;
    bool acceptor_valid;
    Address acceptor;
    size_t syn_len;
    char syn[0];
};

// TRANS -> CM
//
// Send from trans to conn mgr when a connection is transferred,
// reports an error if the connection has already been accepted by an
// application.
struct ConnectionAcceptedEventTransportReply
{
    // true if the connection was claimed, and has subsequently been closed
    // oops_valid_app is NOT valid in this case
    bool is_closed;

    // true if the connection was already claimed (was NOT available
    // for application), and should be removed from the list of
    // pending connections
    bool already_claimed;

    // If already_claimed is true, then it means we have previously
    // sent the connection manager a message telling it that some
    // application timed out and didn't accept the connection, but we
    // were wrong. This field contains the address of the application
    // that accepted the connection, so that it can be added back to
    // the end of the acceptor list for this listening socket.
    Address oops_valid_app;
};

// TRANS -> APP
// CM -> APP
//
// Sent from transport to app when a new connection is available, or
// from conn mgr to app to respond to a setacceptingrequest if none is
// available. (The transport eventually responds if a connection is
// available.)
struct ConnectionAcceptedEventApp
{
    in_addr_t addr;
    uint16_t port;
    transport::flow_t flow_id;
};

// APP -> TRANS
//
// Second stage of handshake. Application gives transport info it
// needs to accept connection.
struct ConnectionAcceptedReply
{
    transport::flow_t flow_id;
    Address conn_sock_addr;
};

// TRANS -> APP
//
// Final stage of handshake. Transport tells application whether or
// not it got the connection. If not, the application calls
// setAccepting again to start the process over.
struct ConnectionAcceptedAck
{
    Status status;
    Address ts_addr;
};

/* Messages sent from the link server to the CM */
struct SynRequest
{
    in_addr_t source;
    uint16_t source_port;
    in_addr_t dest;
    uint16_t dest_port;
    size_t syn_len;
    char syn[0];
};

struct SynReply
{
    int transport_id;
};

// TRANS -> CM
// 
// Sent from transport to CM when an app fails to respond to the
// handshake and we want a new app to try.
// struct TransportApplicationTimeout
// {
//     transport::flow_t listen_sock_id;
//     transport::flow_t flow_id;
//     Connection connection;
//     // FIXME: We have no clue what's happening here. What's a connection?
//     // messaging::Remotebox timed_out_app;
//     // messaging::Remotebox tr_rbox;
// };

// struct TransportApplicationTimeoutReply
// {
//     bool acceptor_valid;
//     // FIXME: Incomplete
//     // messaging::Remotebox acceptor;
// };

// struct BindRequest
// {
//     Address addr;
//     struct in_addr addr;
//     uint16_t port;
//     bool shared;
// };
// 
// struct PerformBindRequest
// {
//     DispatchMessageHeader hdr;
//     DispatchToken app_token;
//     messaging::Remotebox rbox;
//     messaging::Remotebox listen;
//     uint16_t port;
//     struct in_addr addr;
//     bool shared;
// };
// 
// struct BindReply
// {
//     // DispatchMessageHeader hdr;
//     // FosStatus status;
//     // transport::flow_t connection_id;
// };

// struct PeerBindAndListenRequest
// {
//     // FIXME: not implemented yet
//     // transport::flow_t flow_id;
//     // transport::flow_t flow_id_2;
//     // bool is_inaddr_any;
//     // bool is_shared;
// };
// 
// struct PeerBindAndListenReply
// {
//     // FIXME: not implemented yet
//     // FosStatus status;
// };
// 
// struct ConnectRequest
// {
//     // DispatchMessageHeader hdr;
//     // messaging::Remotebox reply_rbox;
//     // messaging::Remotebox client_rbox;
//     // struct in_addr addr;
//     // uint16_t port;
// };
// 
// struct ConnectRequestTransport
// {
//   //   DispatchMessageHeader hdr;
//   //   DispatchToken app_token;
//   //   messaging::Remotebox reply_rbox;
//   //   messaging::Remotebox client_rbox;
//   //   struct in_addr addr;
//   //   uint16_t port;
// }; 
// 
// struct ConnectReply
// {
//     // DispatchMessageHeader hdr;
//     // FosStatus status;
//     // transport::flow_t connection_id;
//     // messaging::Remotebox netstack;
// };
// 
// struct StealConnRequest
// {
//     // DispatchMessageHeader hdr;
//     // transport::flow_t listen_sock_id;
//     // messaging::Remotebox reply_rbox;
//     // messaging::Remotebox cm_rbox; // Should be able to pass in just the index here
// };
// 
// struct StealConnReply
// {
//     // DispatchMessageHeader hdr;
//     // bool is_valid;
//     // Connection conn;
// };

}}}
