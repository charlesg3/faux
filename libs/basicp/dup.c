#define _GNU_SOURCE

#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

int __dup(int oldfd)
{
    int newfd;

    if (!fdIsInternal(oldfd))
    {
        newfd = real_dup(fdExt(oldfd));
        if (newfd > 0) newfd = fdAddExternal(newfd);
    }
    else
    {
        FosFDEntry *file = fdGet(fdInt(oldfd));
        assert(file);
        newfd = fdAddInternal(fdDuplicate(file, -1));

        if(fosIsPipe(file->private_data))
            fsFlattenFdInternal(newfd);
    }

    LT(DUP, "%d -> %d", oldfd, newfd);
    return newfd;
}

int __dup2(int oldfd, int newfd)
{
    LT(DUP, "%d -> %d", oldfd, newfd);
    if (!fdIsInternal(oldfd)) {
        // XXX: make sure to close newfd iff it is managed by Hare since we
        // must keep in sync our fd table with that of the kernel
        // if newfd isn't manged by Hare then the kernel can take care of
        // everything -- siro
        if (fdIsInternal(newfd)) close(newfd);

        // Here's a tricky situation where the underlying representation
        // of the old file descriptor is what we're trying to dup2 onto.
        // In this case, we need to potentially dup the (underlying) new fd
        // onto the oldfd so they point to the same thing.
        // e.g.:
        // old fd = 4, underlying fd = 2
        // case: dup(4, 2)
        //   real_dup(2, 4);
        //   set 4 -> 4 and 2->2
        if(fdExt(oldfd) == newfd)
        {
            if(oldfd == newfd)
            {
                fdSetExternal(newfd, newfd);
                return newfd;
            }
            else
            {
                fdSetExternal(newfd, newfd);
                fdSetExternal(oldfd, oldfd);
                int retfd = real_dup2(newfd, oldfd);
                return retfd;
            }
        }
        else
        {
            int retfd = real_dup2(fdExt(oldfd), newfd);
            if (retfd >= 0) fdSetExternal(retfd, retfd);
            return retfd;
        }
    }

    // XXX: this addresses an issue we found with tar that uses dup2(fd, fd)
    // for some reasons (probabily a validity check) -- siro
    if (oldfd == newfd) return newfd;

    FosFDEntry* oldfile = fdGet(fdInt(oldfd));
    assert(oldfile);

    // XXX: the following code is taken from our close.c and should be kept in
    // sync in case we change the underlying close() implementation -- siro
    // XXX: file may even be NULL but at this point we don't really care since
    // we're going to substitute it anyway -- siro
    if (fdIsInternal(newfd))
    {
        FosFDEntry* newfile = fdGet(fdInt(newfd));
        assert(newfile);
        if (newfile->num_fd == 1)
        {
            assert(newfile->file_ops->close != NULL);
            newfile->file_ops->close(newfile);
        }
        fdDestroy(fdInt(newfd));
        fdRemove(newfd);
    }
    if (fdIsExternal(newfd))
        fdRemove(newfd);

    fdSetInternal(newfd, fdDuplicate(oldfile, -1));

    LT(DUP, "%d (%llx) -> %d", oldfd, fosFileFd(oldfile->private_data), newfd);

    return newfd;
}

// FIXME: implement!
int __dup3(int oldfd, int newfd, int flags)
{
    error(-1, ENOSYS, "dup3()");
    return -ENOSYS;
}

