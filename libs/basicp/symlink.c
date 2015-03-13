#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>

extern InodeType g_cwd_inode;

int __symlinkat(const char* oldpath, int dir_fd, const char* dest)
{
    InodeType at = g_cwd_inode;
    if (dir_fd != AT_FDCWD) {
        // FIXME: Add support for symlink at on underlying fs (passthrough)
        assert(fdIsInternal(dir_fd));
        at = fosFileIno(fdGet(fdInt(dir_fd))->private_data);
    }

    return fosFileSymlink(at, dest, oldpath);
}

int __symlink(const char *oldpath, const char *dest)
{
    return __symlinkat(oldpath, AT_FDCWD, dest);
}

#ifndef VDSOWRAP
int symlinkat(const char* oldpath, int newfddir, const char* newpath)
{
    int retval = __symlinkat(oldpath, newfddir, newpath);
    if (retval) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

int symlink(const char *oldpath, const char *newpath)
{
    int retval = __symlinkat(oldpath, AT_FDCWD, newpath);
    if (retval) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}
#endif

