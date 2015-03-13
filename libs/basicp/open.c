#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>
#include <utilities/debug.h>

extern InodeType g_cwd_inode;
extern FosFileOperations fosFileOps;

int __openat(int dir_fd, const char *path, int flags, mode_t mode)
{
    if (fosPathExcluded(path))
    {
        int fd = real_openat(dir_fd, path, flags, mode);
        return (fd >= 0) ? fdAddExternal(fd) : fd;
    }

    InodeType internal_dir_fd = g_cwd_inode;
    if (dir_fd != AT_FDCWD) {
        if (!fdIsInternal(dir_fd))
        {
            int fd = real_openat(fdExt(dir_fd), path, flags, mode);
            return (fd >= 0) ? fdAddExternal(fd) : fd;
        }
        internal_dir_fd = fosFileIno(fdGet(fdInt(dir_fd))->private_data);
    }

    void* private_data = NULL;
    int retval = fosFileOpen(&private_data, internal_dir_fd, path, flags, mode);
    if (retval < 0)
        goto exit_with_error;
    assert(private_data != NULL);

    FosFDEntry* file = NULL;
    int internal_fd = fdCreate(&fosFileOps, private_data, flags, &file);
    if (internal_fd < 0) {
        retval = -ENOMEM;
        goto exit_with_error;
    }
    assert(file != NULL);

    int fd = fdAddInternal(internal_fd);

    LT(OPEN, "%s -> (intfd: %d) fd: %llx@%d ino: %llx:%llx", path, fd, internal_dir_fd.id, internal_dir_fd.server, fosFileFd(private_data), fosFileIno(private_data).id);
    return fd;
exit_with_error:
    return retval;
}

int __access(const char *path, int mode)
{
    if (fosPathExcluded(path))
        return real_access(path, mode);

    return fosAccess(g_cwd_inode, path, mode);
}

int __creat(const char *path, mode_t mode)
{
    return __openat(AT_FDCWD, path, O_CREAT | O_TRUNC | O_WRONLY, mode);
}

int __faccessat(int dir_fd, const char *path, int mode, int flags)
{
    if (fosPathExcluded(path))
        return real_faccessat(dir_fd == AT_FDCWD ? dir_fd : fdExt(dir_fd), path, mode, flags);

    InodeType internal_dir_fd = g_cwd_inode;
    if (dir_fd != AT_FDCWD)
    {
        if (!fdIsInternal(dir_fd))
            return real_faccessat(fdExt(dir_fd), path, mode, flags);

        internal_dir_fd = fosFileIno(fdGet(fdInt(dir_fd))->private_data);
    }

    return fosAccess(internal_dir_fd, path, mode);
}

int __open(const char *path, int flags, mode_t mode)
{
    return __openat(AT_FDCWD, path, flags, mode);
}

#ifndef VDSOWRAP

int access(const char *path, int mode)
{
    int retval = __access(path, mode);
    if (retval) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

int creat(const char *path, mode_t mode)
{
    int retval = __open(path, O_CREAT | O_TRUNC | O_WRONLY, mode);
    if (retval < 0) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

int faccessat(int dir_fd, const char *path, int mode, int flags)
{
    int retval = __faccessat(dir_fd, path, mode, flags);
    if (retval) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

int creat64(const char *path, mode_t mode)
{
    int retval = __open64(path, O_CREAT | O_TRUNC | O_WRONLY | O_LARGEFILE, mode);
    if (retval < 0) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

int open64(const char *path, int flags, ...)
{
    va_list arg;
    va_start(arg, flags);
    mode_t mode = va_arg(arg, mode_t);
    int retval = __open(path, flags, mode);
    va_end(arg);
    if (retval < 0) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

int open(const char *path, int flags, ...)
{
    va_list arg;
    va_start(arg, flags);
    mode_t mode = va_arg(arg, mode_t);
    int retval = __open(path, flags, mode);
    va_end(arg);
    if (retval < 0) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

int openat(int dirfd, const char *path, int flags, ...)
{
    va_list arg;
    va_start(arg, flags);
    mode_t mode = va_arg(arg, mode_t);
    int retval = __openat(dirfd, path, flags, mode);
    va_end(arg);
    if (retval < 0) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

int openat64(int dirfd, const char *path, int flags, ...)
{
    va_list arg;
    va_start(arg, flags);
    mode_t mode = va_arg(arg, mode_t);
    int retval = __openat(dirfd, path, flags, mode);
    va_end(arg);
    if (retval < 0) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

#endif

