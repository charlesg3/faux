#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>

#include <sys/fs/fs.h>
#include <fd/fd.h>
#include <system/syscall.h>
#include <sys/fs/lib.h>

off64_t __lseek64(int fd, off64_t offset, int whence)
{
    if(!fdIsInternal(fd))
        return real_lseek64(fdExt(fd), offset, whence);

    FosFDEntry *file = fdGet(fdInt(fd));
    if (file == NULL)
    {
        assert(false);
        errno = EBADF;
        return -1;
    }

    assert(file->file_ops->lseek);
    return file->file_ops->lseek(file, offset, whence);
}

long ___llseek(unsigned int fd, unsigned long offset_high, unsigned long offset_low,  loff_t *result,unsigned int whence)
{
    if(!fdIsInternal(fd))
        return real__llseek(fdExt(fd), offset_high, offset_low, result, whence);

    FosFDEntry *file = fdGet(fdInt(fd));
    if (file == NULL)
    {
        assert(false);
        return -EBADF;
    }

    off64_t offset = (((off64_t) offset_high) << 32) | ((off64_t) offset_low);

    assert(file->private_data);

    assert(file->file_ops->lseek);

    *result = (loff_t) file->file_ops->lseek(file, offset, whence);
    return *result == -1 ? (long) *result : 0;
}

#ifndef VDSOWRAP

off64_t lseek64(int fd, off64_t offset, int whence)
{
    ssize_t _errno = __lseek64(fd, offset, whence);

    if(_errno < 0)
    {
        errno = -_errno;
        return -1;
    } else {
        errno = 0;
    }
    return _errno;
}

off_t lseek(int fd, off_t offset, int whence)
{
    return lseek64(fd, offset, whence);
}

#endif


