#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>

#include <system/syscall.h>
#include <sys/fs/lib.h>

extern InodeType g_cwd_inode;

int __rename(const char *oldpath, const char *newpath)
{
    if (fosPathExcluded(oldpath) || fosPathExcluded(newpath))
        return real_rename(oldpath, newpath);
    return fosRename(g_cwd_inode, oldpath, newpath);
}

#ifndef VDSOWRAP
int rename(const char *oldpath, const char *newpath)
{
    return __rename(oldpath, newpath);
}
#endif

