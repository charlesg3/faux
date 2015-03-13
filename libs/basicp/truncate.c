#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern InodeType g_cwd_inode;

int __truncate(const char *path, off_t length)
{
    if (fosPathExcluded(path))
        return real_truncate(path, length);

    return fosTruncate(g_cwd_inode, path, length);
}

int __ftruncate(int fd, off_t length)
{
    if (!fdIsInternal(fd))
        return real_ftruncate64(fdExt(fd), length);

    FosFDEntry *file = fdGet(fdInt(fd));

    return fosFileTruncate(file, length);
}
