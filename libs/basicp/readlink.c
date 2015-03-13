#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern InodeType g_cwd_inode;

ssize_t __readlink(const char *path, char *buf, size_t bufsiz)
{
    if (fosPathExcluded(path))
        return real_readlink(path, buf, bufsiz);

    if (buf == NULL) {
        errno = EFAULT;
        return -1;
    }

    return fosReadlink(g_cwd_inode, path, buf, bufsiz);
}

#ifndef VDSOWRAP
ssize_t readlink(const char *path, char *buf, size_t bufsiz)
{
    return __readlink(path, buf, bufsiz);
}
#endif

