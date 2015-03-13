#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

ssize_t __read(int fd, void *buf, size_t count)
{
    if (!fdIsInternal(fd)) return real_read(fdExt(fd), buf, count);

    if (buf == NULL) return -EFAULT;

    FosFDEntry *file = fdGet(fdInt(fd));
    if (file == NULL || (file->status_flags & O_WRONLY))
        return -EBADF;

    if (file->file_ops->read == NULL) return -EINVAL;

    LT(READ, "%d [%d]", fd, count);
    ssize_t readret = file->file_ops->read(file, buf, count);
    return readret;
}

#ifndef VDSOWRAP

ssize_t read(int fd, void *buf, size_t count)
{
    ssize_t _errno = __read(fd, buf, count);

    if(_errno < 0) {
        errno = -_errno;
        return -1;
    } else {
        errno = 0;
    }
    return _errno;
}

#endif

