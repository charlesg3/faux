#define _GNU_SOURCE
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <syscall.h>
#include <sys/fcntl.h>
#include <assert.h>

#include <system/syscall.h>
#include <fd/fd.h>
#include <sys/fs/lib.h>

// #define SETFL_ALLOWED (O_APPEND | O_ASYNC | O_DIRECT | O_NONBLOCK)
#define SETFL_ALLOWED (O_APPEND | O_ASYNC | O_NONBLOCK | O_CLOEXEC)

int __fcntl(int fd, int cmd, ...)
{
    int result = 0;
    long flags;

    va_list var_args;
    va_start(var_args, cmd);

    if(!fdIsInternal(fd))
    {
        long arg = va_arg(var_args, long);

        // The shell will try to get the flags on an fd to see
        // if it is valid. In this case, we won't have the fd mapped
        // so we let the shell know accordingly.
        if(!fdIsMapped(fd))
            return real_fcntl(fd, cmd, arg);

        return real_fcntl(fdExt(fd), cmd, arg);
    }

    FosFDEntry *file = fdGet(fdInt(fd));
    if (file == NULL)
    {
        fprintf(stderr, "file == NULL: Need to call underlying linux fcntl() here\n");
        assert(0);
        return -EBADF;
    } // if

    switch (cmd)
    {
    case F_DUPFD_CLOEXEC:
        assert(false);
    case F_DUPFD:
        result = dup(fd);
        break;
    case F_GETFD:
        if(file->status_flags & O_CLOEXEC)
            result |= FD_CLOEXEC;
        break;

    case F_SETFD:
        flags = va_arg(var_args, long);
        if(flags & FD_CLOEXEC)
        {
            file->status_flags |= O_CLOEXEC;
            return 0;
        }
        else
            assert(false);
        break;

    case F_GETFL:
        result = file->status_flags;
        break;

    case F_SETFL:
        flags = va_arg(var_args, long);
        file->status_flags = (file->status_flags & ~SETFL_ALLOWED) | (flags & SETFL_ALLOWED);
        break;

    default:
    /*
        fprintf(stderr, "unimplemented fcntl command %d on fd %d\n", cmd, fd);
        */
        break;
    } // switch

    return result;
}

