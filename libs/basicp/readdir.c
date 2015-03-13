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


#ifndef VDSOWRAP
static struct  dirent64 dirent64_ret;

struct dirent64 *readdir64(DIR *dirp)
{
    int fd = dirfd(dirp);
    if(!fdIsInternal(fd)) return real_readdir64(dirp);
    struct dirent *r = fosReadDir(dirp);

    if(!r) return NULL;

    dirent64_ret.d_ino = r->d_ino;
    dirent64_ret.d_off = r->d_off;
    dirent64_ret.d_type = r->d_type;
    dirent64_ret.d_reclen = r->d_reclen;
    strcpy((char *)&dirent64_ret.d_name, r->d_name);

    return &dirent64_ret;
}

struct dirent *readdir(DIR *dirp)
{
    int fd = dirfd(dirp);
    if(!fdIsInternal(fd)) return real_readdir(dirp);
    return (struct dirent *)fosReadDir(dirp);
}

int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result)
{
    fprintf(stderr, "got readdir_r.\n");
    return 0;
}

DIR *opendir(const char *name)
{
    if (fosPathExcluded(name))
        return real_opendir(name);

    void* dir = NULL;
    int retval = fosFileOpen(&dir, g_cwd_inode, name, O_DIRECTORY, 0);
    if (retval < 0)
        goto exit_with_error;
    assert(dir != NULL);

    FosFDEntry *fd_entry = NULL;
    int internal_fd = fdCreate(&fosFileOps, dir, 0, &fd_entry);
    if (internal_fd < 0) {
        retval = -ENOMEM;
        goto exit_with_error;
    }
    assert(fd_entry != NULL);

    fdAddInternal(internal_fd);

    return dir;
exit_with_error:
    errno = -retval;
    return NULL;
}

int closedir(DIR *dirp)
{
    int result = 0;
    int fd = dirfd(dirp);
    assert(fd != -1);

    if(!fdIsInternal(fd)) return real_closedir(dirp);

    FosFDEntry *file = fdGet(fdInt(fd));
    assert(file);

    // close if there are no more fd's pointing to the file
    if (file->num_fd <=1)
    {
        result = fosFreeDir(dirp);
        if (result < 0) errno = -result;
    }

    // We know at this point that the fd is valid, so ignore fdDestroy()'s return value.
    fdDestroy(fdInt(fd));
    fdRemove(fd);
    return (result < 0 ? -1 : 0);
}

int dirfd(DIR *dirp)
{
    int i;
    int fd;
    FosFDEntry *e;
    for(i = 0; i < FD_COUNT; i++)
        if(fdIsInternal(i) && (e = fdGet(fdInt(i))) && dirp == e->private_data)
            return i;

    return real_dirfd(dirp);
}

DIR *fdopendir(int fd)
{
    if(!fdIsInternal(fd))
        return real_fdopendir(fdExt(fd));

    FosFDEntry *file = fdGet(fdInt(fd));
    if(file == NULL)
    {
        errno = EBADF;
        return NULL;
    }

    return (DIR *)file->private_data;
}

#endif
