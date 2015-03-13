#define _GNU_SOURCE

#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <utime.h>

#include <unistd.h>
#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern InodeType g_cwd_inode;

int __utime(const char* path, const struct utimbuf* times)
{
    assert(path);

    if (fosPathExcluded(path))
        return real_utime(path, times);

    return fosUTime(g_cwd_inode, path, times);
}

// XXX: futimens(3) calls sys_utimensat(2) with path set to NULL
// in this condition fd indicates a file and not a directory -- siro
int __utimensat(int dir_fd, const char* path, const struct timespec times[2], int flags)
{
    // XXX: early optimization :D if utimensat() is called and both of times
    // equal UTIME_OMIT we avoid messaging the file system server, this is
    // legal since even Linux returns 0 if this syscall is invoked with both
    // tv_nsec equal to UTIME_OMIT and the pathname doesn't exit -- siro
    if (times && times[0].tv_nsec == UTIME_OMIT && times[1].tv_nsec == UTIME_OMIT)
        return 0;

    LT(ALL, "%d:%s", dir_fd, path);

    if ((path && fosPathExcluded(path)) || (dir_fd != AT_FDCWD && !fdIsInternal(dir_fd)))
        return real_utimensat(dir_fd == AT_FDCWD ? dir_fd : fdExt(dir_fd), path, times, flags);

    InodeType at = dir_fd == AT_FDCWD ? g_cwd_inode
        : fosFileIno(fdGet(fdInt(dir_fd))->private_data);

    return fosUTimeNS(at, path, times, flags);
}

