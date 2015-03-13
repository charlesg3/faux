//Posix Socket Interface
//Author: Charles Gruenwald III
//Date: June 2010
#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

//fos stuff
#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

#define SYS_SOCKET      1               /* sys_socket(2)        */
#define SYS_BIND        2               /* sys_bind(2)          */
#define SYS_CONNECT     3               /* sys_connect(2)       */
#define SYS_LISTEN      4               /* sys_listen(2)        */
#define SYS_ACCEPT      5               /* sys_accept(2)        */
#define SYS_GETSOCKNAME 6               /* sys_getsockname(2)   */
#define SYS_GETPEERNAME 7               /* sys_getpeername(2)   */
#define SYS_SOCKETPAIR  8               /* sys_socketpair(2)    */
#define SYS_SEND        9               /* sys_send(2)          */
#define SYS_RECV        10              /* sys_recv(2)          */
#define SYS_SENDTO      11              /* sys_sendto(2)        */
#define SYS_RECVFROM    12              /* sys_recvfrom(2)      */
#define SYS_SHUTDOWN    13              /* sys_shutdown(2)      */
#define SYS_SETSOCKOPT  14              /* sys_setsockopt(2)    */
#define SYS_GETSOCKOPT  15              /* sys_getsockopt(2)    */
#define SYS_SENDMSG     16              /* sys_sendmsg(2)       */
#define SYS_RECVMSG     17              /* sys_recvmsg(2)       */
#define SYS_ACCEPT4     18              /* sys_accept4(2)       */
#define SYS_RECVMMSG    19              /* sys_recvmmsg(2)      */
#define SYS_SENDMMSG    20              /* sys_sendmmsg(2)      */

int __socketcall(int call, unsigned long *args)
{
    unsigned long a[6];

    if(call < 1 || call > SYS_RECVMSG)
        return -EINVAL;

    switch(call)
    {
    case SYS_SOCKET:
    {
        int fd = real_socketcall(call, args);
        return (fd > 0) ? fdAddExternal(fd) : fd;
    }
    case SYS_CONNECT:
    case SYS_BIND:
    case SYS_GETPEERNAME:
    case SYS_LISTEN:
    case SYS_GETSOCKNAME:
    case SYS_SEND:
    case SYS_RECV:
    case SYS_SENDTO:
    case SYS_RECVFROM:
    case SYS_SHUTDOWN:
    case SYS_SETSOCKOPT:
    case SYS_GETSOCKOPT:
    case SYS_SENDMSG:
    case SYS_RECVMSG:
    case SYS_ACCEPT4:
    case SYS_RECVMMSG:
    case SYS_SENDMMSG:
    {
        memcpy(a, args, sizeof(a));
        a[0] = fdExt(a[0]);
        return real_socketcall(call, a);
    }
    case SYS_ACCEPT:
    {
        memcpy(a, args, sizeof(a));
        a[0] = fdExt(a[0]);
        int fd = real_socketcall(call, args);
        return (fd > 0) ? fdAddExternal(fd) : fd;
    }
    case SYS_SOCKETPAIR:
    {
        return real_socketcall(call, args);
    }
    default:
    {
        fprintf(stderr, "socketcall: %d not handled.\n", call);
        return -1;
        assert(false);
    }
    }
}

#if 0

#ifndef VDSOWRAP
#include <inttypes.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/net/transport.h>

#define SO_SHARED 0x4000
extern FosFileOperations fosSocketOps;

ssize_t recvfrom(int s, void *mem, size_t len, int flags,
      struct sockaddr * __restrict from, socklen_t * __restrict fromlen)
{
    if (mem == NULL)
    {
        errno = EFAULT;
        return -1;
    } // if

    FosFDEntry *fd_entry = fdGet(s);
    if (fd_entry == NULL || (fd_entry->status_flags & O_WRONLY))
    {
        errno = EBADF;
        return -1;
    } // if

    ssize_t result = fosTransportRecvFrom(fd_entry, mem, len, flags, from, fromlen);
    if (result < 0) {
        errno = -result;
        return -1;
    }

    return result;
}

ssize_t send(int s, const void *dataptr, size_t size, int flags)
{
    ssize_t ret; 
    ret = write(s, dataptr, size); 
    return ret; 
}

ssize_t sendto(int s, const void *dataptr, size_t size, int flags,
    const struct sockaddr *to, socklen_t tolen)
{
    if (dataptr == NULL)
    {
        errno = EFAULT;
        return -1;
    } // if

    FosFDEntry *fd_entry = fdGet(s);
    if (fd_entry == NULL || (fd_entry->status_flags & O_WRONLY))
    {
        errno = EBADF;
        return -1;
    } // if

    ssize_t result = fosTransportSendTo(fd_entry, dataptr, size, flags, to, tolen);
    if (result < 0) {
        errno = -result;
        return -1;
    }

    return result;
}


ssize_t
sendmsg(int s, const struct msghdr *msg, int flags)
{
    ssize_t ret;
    size_t tot = 0;
    int i;
    char *buf, *p;
    struct iovec *iov = msg->msg_iov;

    for(i = 0; i < msg->msg_iovlen; ++i)
        tot += iov[i].iov_len;
    buf = malloc(tot);
    if (tot != 0 && buf == NULL) {
        errno = ENOMEM;
        return -1;
    }
    p = buf;
    for (i = 0; i < msg->msg_iovlen; ++i) {
        memcpy (p, iov[i].iov_base, iov[i].iov_len);
        p += iov[i].iov_len;
    }
    
    ret = sendto (s, buf, tot, flags, msg->msg_name, msg->msg_namelen);

    free (buf);
    return ret;
}


int getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    fprintf(stderr, "getsockname called but not implemented.\n");
    return -1;
}

int getsockopt (int s, int level, int optname, void * __restrict optval, socklen_t * __restrict optlen)
{
    fprintf(stderr, "Unimplemented %s\n", __FUNCTION__);
    return -1;
}

static int statusToErrno(FosStatus status)
{
    switch(status)
    {
        case FOS_MAILBOX_STATUS_OK:
            return 0;
        case FOS_MAILBOX_STATUS_ALLOCATION_ERROR:
            errno = ENOMEM;
            return -1;
        case FOS_MAILBOX_STATUS_EMPTY_ERROR:
            errno = EWOULDBLOCK;
            return -1;
        default:
            printf("%s:%s:%d: The status here is %d.\n", __FILE__, __FUNCTION__, __LINE__, status);
            printf("err: %d\n", *(int *)0);
            assert(false);
            return -1;
    }
}

int socket(int domain, int type, int protocol)
{
    int fd;
    void *sock;
    int rc;
    FosFDEntry *fd_entry;

    FosStatus status = fosTransportInitListenSocket(&sock, domain, type, protocol);
    assert(sock);

    if((rc = statusToErrno(status)))
        return rc;

    int flags = O_RDWR;
    if (type & SOCK_NONBLOCK)
        flags |= O_NONBLOCK;

    fd = fdCreate(&fosSocketOps, sock, flags, &fd_entry);
    if (fd < 0)
    {
        assert(false);
        return -1;
    }
    assert(fd_entry->private_data);

    return fd;
}

int getpeer(int s, in_addr_t *addr)
{
    assert(addr);
    FosFDEntry *fd_entry = fdGet(s);
    if (fd_entry == NULL)
    {
        errno = EBADF;
        return -1;
    }

    if (fd_entry->file_ops != &fosSocketOps)
    {
        errno = ENOTSOCK;
        return -1;
    }

    assert(fd_entry->private_data);
    FosStatus status = fosTransportGetPeer(fd_entry, (struct sockaddr *)addr);
    return statusToErrno(status);
}

int setsockopt (int s, int level, int optname, const void *optval, socklen_t optlen)
{
    switch(level)
    {
        case SOL_SOCKET:
            switch(optname)
            {
                case SO_SHARED:
                {
                    FosFDEntry *fd_entry = fdGet(s);
                    if (fd_entry == NULL)
                    {
                        errno = EBADF;
                        return -1;
                    }

                    if (fd_entry->file_ops != &fosSocketOps)
                    {
                        errno = ENOTSOCK;
                        return -1;
                    }


                    assert(fd_entry->private_data);
                    FosStatus status = fosTransportSetShared(fd_entry, optval);
                    return statusToErrno(status);
                }
                default:
                    break;
            }
        default: 
            break;

    }
#ifdef WARN_UNIMPLEMENTED
    fprintf(stderr, "Unimplemented %s\n", __FUNCTION__);
#endif
    return 0;
}

int connect(int s, const struct sockaddr *name, socklen_t namelen)
{
    if(name->sa_family != AF_INET && name->sa_family != AF_UNSPEC)
    {
        assert(false);
        return -1;
    }

    FosFDEntry *fd_entry = fdGet(s);
    if (fd_entry == NULL)
    {
        errno = EBADF;
        return -1;
    }

    if (fd_entry->file_ops != &fosSocketOps)
    {
        errno = ENOTSOCK;
        return -1;
    }

    struct sockaddr_in *sin = (struct sockaddr_in *)name;

    assert(fd_entry->private_data);
    FosStatus status = fosTransportConnect(fd_entry, name, namelen);
    return statusToErrno(status);
}

int bind(int s, const struct sockaddr *name, socklen_t namelen)
{
    if (name->sa_family != AF_INET && name->sa_family != AF_UNSPEC)
    {
        printf("unknown family: %d we only know: %d\n", name->sa_family, AF_INET);
        errno = EAFNOSUPPORT;
        return -1;
    }

    FosFDEntry *fd_entry = fdGet(s);
    if (fd_entry == NULL)
    {
        errno = EBADF;
        return -1;
    }

    if (fd_entry->file_ops != &fosSocketOps)
    {
        errno = ENOTSOCK;
        return -1;
    }

    assert(fd_entry->private_data);

    FosStatus status = fosTransportBind(fd_entry, name, namelen);
    return statusToErrno(status);
}

int listen(int s, int backlog)
{
    FosFDEntry *fd_entry = fdGet(s);
    if (fd_entry == NULL)
    {
        errno = EBADF;
        return -1;
    }

    if (fd_entry->file_ops != &fosSocketOps)
    {
        errno = ENOTSOCK;
        return -1;
    }

    assert(fd_entry->private_data);
    FosStatus status = fosTransportListen(fd_entry, backlog);
    return statusToErrno(status);
}

int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    return accept4(s, addr, addrlen, 0);
}

int accept4(int s, struct sockaddr *addr, socklen_t *addrlen, int flags)
{
    int fd = -1;

    FosFDEntry *file = fdGet(s);
    if (file == NULL)
    {
        fprintf(stderr, "error looking up fd that we're accepting on: %d\n", s);
        while(1);
        errno = EBADF;
        return -1;
    }
    assert(file->private_data);

    void *sock;
    FosStatus status = fosTransportAccept(file, &sock, addr, addrlen, flags);
    if(statusToErrno(status))
        return -1;
    assert(sock);

    FosFDEntry *fd_entry;
    fd = fdCreate(&fosSocketOps, sock, O_RDWR, &fd_entry);
    if (fd < 0)
    {
        assert(false); // FIXME: in this situation we have already accepted the connection
                       // however we have no fd for it. Not sure how to handle this.
        return -1;
    }

    return fd;
}

#endif
#endif
