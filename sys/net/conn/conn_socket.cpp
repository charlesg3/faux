#include <algorithm>

#include "transport_private.hpp"
#include "conn_private.hpp"
#include "conn_listen.hpp"
#include "conn_socket.hpp"

namespace fos { namespace net { namespace conn {

// **** ListenSocket class
// 
// This class manages two lists: a list of valid acceptors and a list
// of pending connections.
// 
// This is NOT part of the application, it lives in the listening
// netstack.

ListenSocket::ListenSocket()
    : Socket(LISTEN_SOCKET)
    , m_connection_id(0)
{
}

ListenSocket::~ListenSocket()
{
}

bool ListenSocket::getAcceptor(Address * acceptor)
{
    if (m_acceptors.empty())
        return false;

    *acceptor = m_acceptors.front();

    // Re-prioritize this as unlikely, since it just got a connection
    m_acceptors.pop_front();
    m_acceptors.push_back(*acceptor);

    return true;
}

void ListenSocket::addAcceptor(const Address & acceptor, bool likely_hint)
{
    AcceptorList::iterator i = std::find(m_acceptors.begin(), m_acceptors.end(), acceptor);
    if (i == m_acceptors.end())
    {
        if (likely_hint)
            m_acceptors.push_front(acceptor);
        else
            m_acceptors.push_back(acceptor);
    }
}
#if 0

void ListenSocket::removeAcceptor(const Remotebox & acceptor)
{
    AcceptorList::iterator i = std::find(m_acceptors.begin(), m_acceptors.end(), acceptor);
    if (i != m_acceptors.end())
        m_acceptors.erase(i);
}
#endif

bool ListenSocket::popPendingConnection(Connection *connection)
{
    if (m_connections.empty())
        return false;

    *connection = m_connections.front();
    m_connections.pop_front();

    return true;
}

void ListenSocket::pushPendingConnection(const Connection & connection)
{
    m_connections.push_back(connection);
}

bool ListenSocket::getThief(Channel **thief)
{
    if(m_thieves.empty())
        return false;

    *thief = m_thieves.front();

    // Move the thief to the end of the list (round-robin among thieves)
    m_thieves.pop_front();
    m_thieves.push_back(*thief);

    return true;
}

void ListenSocket::addThief(Channel *thief)
{
    Thieves::iterator it = std::find(m_thieves.begin(), m_thieves.end(), thief);
    if(it == m_thieves.end())
        m_thieves.push_front(thief);
}

void ListenSocket::removeThief(Channel *thief)
{
    Thieves::iterator it = std::find(m_thieves.begin(), m_thieves.end(), thief);
    if(it != m_thieves.end())
        m_thieves.erase(it);
}

void ListenSocket::setConnectionId(transport::flow_t conn_id)
{
    m_connection_id = conn_id;
}

transport::flow_t ListenSocket::getConnectionId()
{
    return m_connection_id;
}

MessageSocket::MessageSocket()
    : Socket(MESSAGE_SOCKET)
    , m_connection_id(0)
{
}

MessageSocket::~MessageSocket()
{
}

void MessageSocket::setConnectionId(transport::flow_t conn_id)
{
    m_connection_id = conn_id;
}

transport::flow_t MessageSocket::getConnectionId()
{
    return m_connection_id;
}

}}} // namespaces
