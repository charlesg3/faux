#define _GNU_SOURCE

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern InodeType g_cwd_inode;

int __chmod(const char *path, mode_t mode)
{
    if(fosPathExcluded(path)) return real_chmod(path, mode);
    return fosChmod(g_cwd_inode, path, mode);
}

int __fchmod(int fd, mode_t mode)
{
    InodeType at = g_cwd_inode;
    if (!fdIsInternal(fd))
        return real_fchmod(fdExt(fd), mode);
    else
        at = fosFileIno(fdGet(fdInt(fd))->private_data);

    return fosChmod(at, NULL, mode);
}

int __fchmodat(int dir_fd, const char *path, mode_t mode, int flags)
{
    if(fosPathExcluded(path))
        return real_fchmodat(dir_fd == AT_FDCWD ? dir_fd : fdExt(dir_fd), path, mode);
    assert(dir_fd == AT_FDCWD);
    return fosChmod(g_cwd_inode, path, mode);
}
