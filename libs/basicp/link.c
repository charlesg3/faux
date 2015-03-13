#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>

int link(const char *oldpath, const char *newpath)
{
    assert(false);
    errno = ENOSYS;
    return -1;
}
