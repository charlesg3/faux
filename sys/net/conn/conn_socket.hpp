#pragma once

#include <list>

#include "conn_private.hpp"
#include "transport_private.hpp"
#include "transport_socket.hpp"
// #include <messaging/messaging.hpp>

namespace fos { namespace net { namespace conn {

// Different types of sockets
// FIXME: These are 0x4 and 0x5 because the code in transport servers relies on
// these values being different from FLOW_SOCKET etc. This dependence needs to
// be removed
enum SocketType {
    LISTEN_SOCKET = 0x1,
    MESSAGE_SOCKET = 0x2,
};

// Generic base class to let us catch when requests go to the
// wrong type of socket
class Socket
{
public:
    Socket(SocketType type) : m_type(type) {}
    SocketType type() const { return m_type; }

private:
    SocketType m_type;
};

// This code lives in the connection manager, netstack that handles
// listening sockets
class ListenSocket : public Socket
{
public:
    ListenSocket();
    ~ListenSocket();

    bool getAcceptor(Address * acceptor); // implicitly bumps down this acceptor's priority
    void addAcceptor(const Address & acceptor, bool likely_hint = true); // hint says whether we think this is a likely (ie, high priority) acceptor
    void removeAcceptor(const Address & acceptor); // remove a truant acceptor

    bool getThief(Channel **thief);
    void addThief(Channel *thief);
    void removeThief(Channel *thief);

    bool popPendingConnection(Connection * connection); // implicitly removes connection from list
    void pushPendingConnection(const Connection & connection); // for connections when no acceptor is available

    void setConnectionId(transport::flow_t conn_id);
    transport::flow_t getConnectionId();

private:

    typedef std::list<Address> AcceptorList;
    typedef std::list<Channel *> Thieves;
    typedef std::list<Connection> ConnectionList;

    AcceptorList m_acceptors;
    Thieves m_thieves;
    ConnectionList m_connections;
    transport::flow_t m_connection_id;
};

class MessageSocket : public Socket
{
public:
    MessageSocket();
    ~MessageSocket();

    void setConnectionId(transport::flow_t conn_id);
    transport::flow_t getConnectionId();

private:
    transport::flow_t m_connection_id;
};

}}}
