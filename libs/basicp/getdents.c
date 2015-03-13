#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <utilities/debug.h>
#include <system/syscall.h>

extern FosFileOperations fosFileOps;
extern file_id_t g_cwd_inode;

int __getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count)
{
    if(!fdIsInternal(fd)) return real_getdents(fdExt(fd), dirp, count);

    if (dirp == NULL) return -EFAULT;

    FosFDEntry *file = fdGet(fdInt(fd));
    assert(file);

    int ret = fosGetDents(file->private_data, dirp, count);
    return ret;
}

int __getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count)
{
    if(!fdIsInternal(fd)) return real_getdents64(fdExt(fd), dirp, count);

    if (dirp == NULL) return -EFAULT;

    FosFDEntry *file = fdGet(fdInt(fd));
    assert(file);

    int ret = fosGetDents64(file->private_data, dirp, count);
    return ret;
}

