#define _GNU_SOURCE

#include <errno.h>
#include <unistd.h>

#include <sys/fs/lib.h>

extern InodeType g_cwd_inode;

int __rmdir(const char *path)
{
    return fosRmdir(g_cwd_inode, path);
}

#ifndef VDSOWRAP
int rmdir(const char* path)
{
    int retval = __rmdir(path);
    if (retval) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}
#endif

