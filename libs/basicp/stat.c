#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern InodeType g_cwd_inode;

int __stat(const char *path, struct stat *buf)
{
    LT(FSTAT, "%s", path);
    // See if this path should be filtered
    if(fosPathExcluded(path)) return real_stat(path, buf);
    return fosFStatAt(g_cwd_inode, path, buf, true);
}

int __stat64(const char *path, struct stat64 *buf)
{
    // See if this path should be filtered
    if(fosPathExcluded(path))
    {
        int ret = real_stat64(path, buf);
        return ret;
    }
    const char *realpath_s = strcmp(path,".") == 0 ? "" : path;
    return fosFStatAt64(g_cwd_inode, realpath_s, buf, true);
}

int __lstat64(const char *path, struct stat64 *buf)
{
    // See if this path should be filtered
    if(fosPathExcluded(path)) return real_lstat64(path, buf);
    const char *realpath_s = strcmp(path,".") == 0 ? "" : path;
    return fosFStatAt64(g_cwd_inode, realpath_s, buf, false);
}

int __fstat(int fd, struct stat *buf)
{
    if(!fdIsInternal(fd))
        return real_fstat(fdExt(fd), buf);

    FosFDEntry *file = fdGet(fdInt(fd));
    assert(file);
    InodeType inode = fosFileIno(file->private_data);

    return fosFStatAt(inode, "", buf, true);
}

int __fstatat64(int dir_fd, const char *path, struct stat64 *buf, int flags)
{
    if(fosPathExcluded(path)) return real_fstatat64(dir_fd == AT_FDCWD ? dir_fd : fdExt(dir_fd), path, buf, flags);

    InodeType dir_fd_i = g_cwd_inode;
    if(dir_fd != AT_FDCWD && dir_fd != 0)
    {
        if(!fdIsInternal(dir_fd))
            return real_fstatat64(fdExt(dir_fd), path, buf, flags);
        FosFDEntry *file = fdGet(fdInt(dir_fd));
        assert(file);
        dir_fd_i = fosFileIno(file->private_data);
    }
    return fosFStatAt64(dir_fd_i, path, buf, false);
}

int __fstat64(int fd, struct stat64 *buf)
{
    if(!fdIsInternal(fd)) return real_fstat64(fdExt(fd), buf);

    FosFDEntry *file = fdGet(fdInt(fd));
    assert(file);
    InodeType inode = fosFileIno(file->private_data);

    return fosFStatAt64(inode, "", buf, true);
}

int __lstat(const char *path, struct stat *buf)
{
    if(fosPathExcluded(path)) return real_lstat(path, buf);
    return fosFStatAt(g_cwd_inode, path, buf, false);
}

#ifndef VDSOWRAP

int stat(const char *path, struct stat *buf)
{
    return __stat(path, buf);
}

int stat64(const char *path, struct stat64 *buf)
{
    return __stat64(path, buf); }

int lstat64(const char *path, struct stat64 *buf)
{
    return __lstat64(path, buf);
}

int fstat(int fd, struct stat *buf)
{
    return __fstat(fd, buf);
}

int fstatat(int dir_fd, const char *path, struct stat *buf, int flags)
{
    if (!buf) {
        errno = EFAULT;
        return -1;
    }
    struct stat64 buf64;
    int retval = __fstatat64(dir_fd, path, &buf, flags);
    if (retval) {
        errno = -retval;
        retval = -1;
    } else {
        buf->st_dev = buf64.st_dev;
        buf->st_ino = buf64.st_ino;
        buf->st_mode = buf64.st_mode;
        buf->st_nlink = buf64.st_nlink;
        buf->st_uid = buf64.st_uid;
        buf->st_gid = buf64.st_gid;
        buf->st_rdev = buf64.st_rdev;
        buf->st_blocks = buf64.st_blocks;
        buf->st_blksize = buf64.st_blksize;
        buf->st_size = buf64.st_size;
        buf->st_atime = buf64.st_atime;
        buf->st_mtime = buf64.st_mtime;
        buf->st_ctime = buf64.st_ctime;
        buf->st_mode = buf64.st_mode;
    }
    return retval;
}

int fstatat64(int dir_fd, const char *path, struct stat64 *buf, int flags)
{
    return __fstatat64(dir_fd, path, buf, flags);
}

int fstat64(int fd, struct stat64 *buf)
{
    return __fstat64(fd, buf);
}

int lstat(const char *path, struct stat *buf)
{
    return __lstat(path, buf);
}

int __lxstat64(int stat_ver, const char *path, struct stat64 *buf)
{
    return __lstat64(path, buf);
}

int __xstat64(int stat_ver, const char *path, struct stat64 *buf)
{
    return __stat64(path, buf);
}

int __xstat(int stat_ver, const char *path, struct stat *buf)
{
    return __stat(path, buf);
}

int __lxstat(int stat_ver, const char *path, struct stat *buf)
{
    return __lstat(path, buf);
}

int __fxstat(int stat_ver, int fd, struct stat *buf)
{
    return __fstat(fd, buf);
}

int __fxstatat(int stat_ver, int dir_fd, const char *path, struct stat *buf, int flags)
{
    if (!buf)
        return -EFAULT;
    struct stat64 buf64;
    int retval = __fstatat64(dir_fd, path, &buf, flags);
    if (!retval) {
        buf->st_dev = buf64.st_dev;
        buf->st_ino = buf64.st_ino;
        buf->st_mode = buf64.st_mode;
        buf->st_nlink = buf64.st_nlink;
        buf->st_uid = buf64.st_uid;
        buf->st_gid = buf64.st_gid;
        buf->st_rdev = buf64.st_rdev;
        buf->st_blocks = buf64.st_blocks;
        buf->st_blksize = buf64.st_blksize;
        buf->st_size = buf64.st_size;
        buf->st_atime = buf64.st_atime;
        buf->st_mtime = buf64.st_mtime;
        buf->st_ctime = buf64.st_ctime;
        buf->st_mode = buf64.st_mode;
    }
    return retval;
}

int __fxstat64(int stat_ver, int fd, struct stat64 *buf)
{
    return __fstat64(fd, buf);
}

int __fxstatat64(int stat_ver, int dir_fd, const char *path, struct stat64 *buf, int flags)
{
    return __fstatat64(dir_fd, path, buf, flags);
}

#endif

