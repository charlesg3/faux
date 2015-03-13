#pragma once
#include <sys/socket.h>
#include <sys/types.h>

#include <system/status.h>

#include <fd/fd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRANSPORT_NAME 42
#define CONN_MANAGER_NAME 1008

/**
 * @brief Create a socket.
 * @see man 3 socket
 */
int fosTransportSocket(struct FosFDEntry *file, int in_type, int in_protocol);

/**
 * @brief Write to a socket.
 * @see man 3 write
 */
ssize_t fosTransportWrite(struct FosFDEntry *file, const void * in_buf, size_t in_count);

/**
 * @brief Read from a socket.
 * @see man 3 write
 */
ssize_t fosTransportRead(struct FosFDEntry *file, const void * in_buf, size_t in_count);

    /* @todo These should probably return FosStatus. -nzb */

/**
 * @brief Connect a socket.
 * @see man 3 connect
 */
int fosTransportConnect(struct FosFDEntry *file, const struct sockaddr * in_address, socklen_t in_address_len);

/**
 * @brief Bind a socket to a name.
 * @see man 3 bind
 */
int fosTransportBind(struct FosFDEntry *file, const struct sockaddr * in_address, socklen_t in_address_len);

/**
 * @brief Listen on a socket.
 * @see man 3 listen
 */
int fosTransportListen(struct FosFDEntry *file, int in_backlog);

/**
 * @brief Accept a connection from a listening socket.
 * @see man 3 accept. The void **vp is an output parameter that stores
 * the created Socket.
 *
 * @param vp Output paramater for newly accepted socket.
 */
int fosTransportAccept(struct FosFDEntry *file, void **vp, struct sockaddr * in_address, socklen_t * in_address_len, int flags);

/**
 * @brief Get host by name.
 * @see mane 3 gethostbyname.
 */
FosStatus fosTransportGetHostByName(struct in_addr * out_addr, const char * in_name);

/**
 * @brief Initialize Socket
 * This will allocate a new socket. The higher layer (basicp) calls
 * this and assumes Socket is an opaque object, thus the void**vp
 * is an output parameter that is filled in with the socket structure.
 * @param vp Output parameter to be set to the opaque socket object
 */
FosStatus fosTransportInitListenSocket(void **vp, int domain, int type, int protocol);
FosStatus fosTransportInitFlowSocket(void **vp, int domain, int type, int protocol);

/**
 * @brief Put the socket in a shared state
 * This will allow the specified socket to be shared amongst separate processes. Note
 * this must only be called on sockets that will be used for listening and before the
 * listen() call is made.
 * @param file The socket to operate on
 * @param shared The setting to use, true = shared
 */
FosStatus fosTransportSetShared(struct FosFDEntry *file, bool shared);

/**
 * @brief Get the ip address of the remote end of a socket.
 * @param file The socket to operate on
 * @param addr The output parameter to store the address in.
 */
int fosTransportGetPeer(struct FosFDEntry *file, struct sockaddr * out_address);

/**
 * @brief receive a udp packet and store sender in result.
 * @see man 2 recvfrom
 */
FosStatus fosTransportRecvFrom(struct FosFDEntry *file, void *mem, size_t len, int flags,
      struct sockaddr * __restrict from, socklen_t * __restrict fromlen);
/**
 * @brief Send a datagram packet to a specific host
 * @see man 2 sendto
 */
FosStatus fosTransportSendTo(struct FosFDEntry *file, const void *dataptr, size_t size, int flags,
    const struct sockaddr *to, socklen_t tolen);
#ifdef __cplusplus
}
#endif
