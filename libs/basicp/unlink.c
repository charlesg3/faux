#define _GNU_SOURCE

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern InodeType g_cwd_inode;

int __unlink(const char *path)
{
    if(fosPathExcluded(path)) return real_unlink(path);

    return fosUnlink(g_cwd_inode, path);
}

int __unlinkat(int dir_fd, const char *path, int flags)
{
//    if (fosPathExcluded(path))
//        return real_unlinkat(dir_fd, path, flags, mode);

    InodeType internal_dir_fd = g_cwd_inode;
    if (dir_fd != AT_FDCWD) {
        if (!fdIsInternal(dir_fd))
            return real_unlinkat(dir_fd == AT_FDCWD ? dir_fd : fdExt(dir_fd), path, flags);

        internal_dir_fd = fosFileIno(fdGet(fdInt(dir_fd))->private_data);
    }

    if(flags & AT_REMOVEDIR)
        return fosRmdir(internal_dir_fd, path);
    else
        return fosUnlink(internal_dir_fd, path);
}

#ifndef VDSOWRAP
int unlink(const char* path)
{
    return __unlink(path);
}

int unlinkat(int dir_fd, const char* path, int flags)
{
    return __unlink(dir_fd, path, flags);
}
#endif

