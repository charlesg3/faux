#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern InodeType g_cwd_inode;
extern bool g_cwd_in;

long __getcwd(char *buf, unsigned long size)
{
    if (!g_cwd_in) return real_getcwd(buf, (size_t) size);
    if (buf == NULL) return -EFAULT;

    // XXX: sys_getcwd() returns the length of the string representing the CWD
    // +1, i.e., the number of bytes written in buf -- siro

    size_t retsize = size; 
    int ret = fosResolveFull(g_cwd_inode, buf, &retsize);

    return ret == OK ? retsize : ret;
}

