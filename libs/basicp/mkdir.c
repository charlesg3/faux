#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>
#include <system/status.h>

extern InodeType g_cwd_inode;

int __mkdir(const char *path, mode_t mode)
{
    if (fosPathExcluded(path))
        return real_mkdirat(AT_FDCWD, path, mode);

    return fosMkdir(g_cwd_inode, path, mode);
}

int __mkdirat(int dir_fd, const char *path, mode_t mode)
{
    if (fosPathExcluded(path))
        return real_mkdirat(dir_fd == AT_FDCWD ? dir_fd : fdExt(dir_fd), path, mode);

    InodeType internal_dir_fd = g_cwd_inode;
    if (dir_fd != AT_FDCWD) {
        if (!fdIsInternal(dir_fd))
            return real_mkdirat(fdExt(dir_fd), path, mode);
        internal_dir_fd = fosFileIno(fdGet(fdInt(dir_fd))->private_data);
    }

    return fosMkdir(internal_dir_fd, path, mode);
}
