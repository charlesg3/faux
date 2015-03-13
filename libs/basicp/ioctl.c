#define _GNU_SOURCE
#include <stdarg.h>
#include <sys/ioctl.h>

#include <system/syscall.h>
#include <fd/fd.h>
#include <sys/fs/lib.h>

int __ioctl(int fd, unsigned long int request, ...)
{
    va_list var_args;
    va_start(var_args, request);

    if(!fdIsInternal(fd))
    {
        long arg = va_arg(var_args, long);
        return real_ioctl(fdExt(fd), request, arg);
    }

    switch(request)
    {
    case TCGETS:
        return -1;
    default:
    {
        long arg = va_arg(var_args, long);
        return real_ioctl(fdInt(fd), request, arg);
    }
    }
}

