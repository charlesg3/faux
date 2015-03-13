#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

ssize_t __write(int fd, const void * buf, size_t count)
{
    if (!fdIsInternal(fd)) return real_write(fdExt(fd), buf, count);
    if (buf == NULL) return -EFAULT;

    FosFDEntry *file = fdGet(fdInt(fd));
    assert(file);
    if (file->status_flags & O_RDONLY) return -EBADF;

    if (file->file_ops->write == NULL) return -EINVAL;

    LT(WRITE, "%d [%d]", fd, count);
    ssize_t ret = file->file_ops->write(file, buf, count);
    return ret;
}

#ifndef VDSOWRAP

ssize_t write(int fd, const void * buf, size_t count)
{
    ssize_t _errno = __write(fd, buf, count);

    if(_errno < 0) {
        errno = -_errno;
        return -1;
    } else {
        errno = 0;
    }
    return _errno;

}

#endif

