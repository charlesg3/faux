#define _GNU_SOURCE

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>
#include <utilities/debug.h>

extern bool g_faux_initted;

int __close_int(int fd)
{
    int internal_fd = fdInt(fd);
    FosFDEntry *file = fdGet(internal_fd);
    assert(file);

    LT(CLOSE, "(%d) %llx", fd, fosFileFd(file->private_data));

    int retval = 0;
    // XXX: check the reference count to see how many file descriptors point to
    // the file; if the number of file descripts has dropped to 1 then proceed
    // closing the file -- siro
    if (file->num_fd == 1) {
        assert(file->file_ops->close != NULL);
        retval = file->file_ops->close(file);
        if (retval == 0) {
            errno = 0;
        } else {
            errno = -retval;
            retval = -1;
        }
    }

    fdDestroy(internal_fd);

    fdRemove(fd);

    return retval;
}

int __close(int fd)
{
    if(fd < 0) return -EBADF;
    if(fd > 2 && !fdIsMapped(fd)) return -EBADF;

    if (!g_faux_initted || !fdIsInternal(fd))
    {
        int ret = real_close(fdExt(fd));
        fdRemove(fd);
        return ret;
    }

    LT(CLOSE,"%d (int: %d)", fd, fdInt(fd));

    int retval = __close_int(fd);
    return retval;
}

#ifndef VDSOWRAP
int close(int fd)
{
    int retval = __close(fd);
    if (retval) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}
#endif

