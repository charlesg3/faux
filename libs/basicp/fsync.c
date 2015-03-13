#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern file_id_t g_cwd_inode;

int __fsync(int fd)
{
    if (!fdIsInternal(fd))
        return real_fsync(fdExt(fd));

    return 0;
}

int __fdatasync(int fd)
{
    if (!fdIsInternal(fd))
        return real_fdatasync(fdExt(fd));

    return 0;
}

