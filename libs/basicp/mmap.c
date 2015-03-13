#define _GNU_SOURCE
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>

#include <fd/fd.h>
#include <utilities/debug.h>
#include <system/syscall.h>
#include <sys/fs/lib.h>

extern off64_t __lseek64(int fd, off64_t offset, int whence);

void *__mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    if(fd == -1)
        return (void *)real_mmap2(addr, length, prot, flags, fd, offset);
    if(flags == (MAP_SHARED|MAP_ANONYMOUS))
        return (void *)real_mmap2(addr, length, prot, flags, fd, offset);
    if(!fdIsInternal(fd))
        return (void *)real_mmap2(addr, length, prot, flags, fdExt(fd), offset);

    // we only support read only, private file-backed mmaps
    // assert(prot == PROT_READ || prot == PROT_EXEC);
    if (prot & PROT_WRITE)
        fprintf(stderr, "mmap(): Hare doesn't support PROT_WRITE! Dazed and confused, but trying to continue...\n");

    // malloc the memory
    //NOTE: Here we map the memory as PROT_WRITE as well so that we can fill in the data with
    //the file... The application could technically now write to that address, though they
    //would normally receive a SEGFAULT error, in our case the error goes undetected. This
    //would be an incorrect program in any case.
    //--cg3
    void *retaddr = (void *)real_mmap2(addr, length, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|flags, -1, 0);

    // fill contents with file contents
    off64_t off __attribute__((unused));
    off = __lseek64(fd, offset * 4096, SEEK_SET);
    assert(off == offset * 4096);

    FosFDEntry *file = fdGet(fdInt(fd));
    if (file == NULL || (file->status_flags & O_WRONLY))
        assert(false);

    assert(file->file_ops->read);

    ssize_t readlen __attribute__((unused));
    readlen = file->file_ops->read(file, retaddr, length);

    // make sure we read the whole file
    assert(readlen == length);

    return retaddr;
}

int __munmap(void *addr, size_t length)
{
    return real_munmap(addr, length);
}
