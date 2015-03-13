#define _GNU_SOURCE

#include <sys/types.h>
#include <errno.h>

#include <system/syscall.h>
#include <sys/fs/lib.h>
#include <fd/fd.h>

/*
int __setxattr(const char *path, const char *name,
                      const void *value, size_t size, int flags)
{
}

int __lsetxattr(const char *path, const char *name,
                      const void *value, size_t size, int flags)
{
}
*/
int __fsetxattr(int fd, const char *name,
                      const void *value, size_t size, int flags)
{
    if (!fdIsInternal(fd)) return real_fsetxattr(fdExt(fd), name, value, size, flags);

    errno = ENOTSUP;
    return 0;
}

