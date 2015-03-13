#include <stdlib.h>
#include <errno.h>
#include <selinux/selinux.h>

int getfilecon(const char *path, security_context_t *con)
{
    *con = NULL;
    errno = ENOTSUP;
    return 0;
}

int lgetfilecon(const char *path, security_context_t *con)
{
    *con = NULL;
    errno = ENOTSUP;
    return 0;
}

int fgetfilecon(int fd, security_context_t *con)
{
    *con = NULL;
    errno = ENOTSUP;
    return 0;
}

