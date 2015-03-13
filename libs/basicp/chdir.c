#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <dlfcn.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern InodeType g_cwd_inode;
extern bool g_cwd_in;

int __chdir(const char *path)
{
    LT(CHDIR, "%s", path);
    if (fosPathExcluded(path) || !g_cwd_in) {
        g_cwd_in = false;
        return real_chdir(path);
    }

    //make sure path exists
    bool cached = false;
    InodeType inode;
    if (fosLookup(g_cwd_inode, path, &inode, &cached) != OK) {
        g_cwd_in = false;
        return -ENOENT;
    } else {
        g_cwd_in = true;
        g_cwd_inode = inode;
    }

    return 0;
}

int __fchdir(int fd)
{
    LT(CHDIR, "%d", fd);
    if(!fdIsInternal(fd))
    {
        g_cwd_in = false;
        return real_fchdir(fdExt(fd));
    }

    g_cwd_in = true;

    FosFDEntry *file = fdGet(fdInt(fd));
    assert(fosIsDir(file->private_data));
    g_cwd_inode = fosFileIno(file->private_data);

    return 0;
}
