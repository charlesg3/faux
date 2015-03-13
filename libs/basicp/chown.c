#define _GNU_SOURCE

#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern InodeType g_cwd_inode;

int __fchownat(int dir_fd, const char *path, uid_t owner, uid_t group)
{
    if (fosPathExcluded(path))
        return real_fchownat(dir_fd == AT_FDCWD ? dir_fd : fdExt(dir_fd), path, owner, group);

    InodeType at = g_cwd_inode;
    if (dir_fd != AT_FDCWD) {
        if (!fdIsInternal(dir_fd))
            return real_fchownat(fdExt(dir_fd), path, owner, group);
        at = fosFileIno(fdGet(fdInt(dir_fd))->private_data);
    }

    return fosChown(at, path, owner, group);
}

int __chown(const char *path, uid_t owner, gid_t group)
{
    return __fchownat(AT_FDCWD, path, owner, group);
}

int __lchown(const char *path, uid_t owner, gid_t group)
{
    return __fchownat(AT_FDCWD, path, owner, group);
}

int __fchown(int fd, uid_t owner, gid_t group)
{
    InodeType inode = fosFileIno(fdGet(fdInt(fd))->private_data);
    return fosChown(inode, "", owner, group);
}


