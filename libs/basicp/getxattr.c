#include <sys/types.h>
#include <errno.h>
//#include <attr/xattr.h>

ssize_t getxattr(const char *path, const char *name, void *value, size_t size)
{
    errno = ENOTSUP;
    return 0;
}

ssize_t lgetxattr(const char *path, const char *name, void *value, size_t size)
{
    errno = ENOTSUP;
    return 0;
}

ssize_t fgetxattr(int fd, const char *name, void *value, size_t size)
{
    errno = ENOTSUP;
    return 0;
}


