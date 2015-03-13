#pragma once

#include "conn_private.hpp"

namespace fos { namespace net { namespace app {

    class Socket;

    void setAccepting(Socket * socket);
    bool acceptConnection(Socket * listen_socket, Socket * & connection_socket, const conn::ConnectionAcceptedEventApp * event, Channel *chan);

}}}

