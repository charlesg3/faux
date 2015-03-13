#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fd/fd.h>
#include <sys/fs/fs.h>
#include <sys/fs/lib.h>
#include <utilities/debug.h>

#include <sys/fs/interface.h>

#include "pipe.hpp"
#include "file.hpp"
#include "dir.hpp"

using namespace fos;
using namespace fos::fs;
using namespace fos::fs::app;

ssize_t fosFileRead(struct FosFDEntry *file, void *buf, size_t nbytes)
{
    VFile *vf = (VFile *)file->private_data;
    assert(vf);
    if(vf->isPipe())
    {
        auto p = (Pipe *)vf;
        return p->read(buf, nbytes);
    }
    else
    {
        auto f = (File *)vf;
        return f->read(buf, nbytes);
    }
}

int fosFilePoll(struct FosFDEntry *file)
{
    File *f = (File *)file->private_data;
    assert(f);
    return f->poll();
}

off_t fosFileLseek(struct FosFDEntry *file, off_t offset, int whence)
{
    File *f = (File *)file->private_data;
    assert(f && f->isFile());
    return f->lseek((off64_t)offset, whence);
}

ssize_t fosFileWrite(struct FosFDEntry *file, const void * in_buf, size_t in_count)
{
    VFile *vf = (VFile *)file->private_data;
    assert(vf);
    if(vf->isPipe())
    {
        auto p = (Pipe *)vf;
        return p->write(in_buf, in_count);
    }
    else
    {
        auto f = (File *)vf;
        return f->write(in_buf, in_count);
    }
}

int fosFileClose(struct FosFDEntry *file)
{
    VFile *vf = (VFile *)file->private_data;
    if(vf->isDir())
    {
        auto d = (Dir *)vf;
        d->close();
        delete d;
    }
    else if(vf->isPipe())
    {
        auto p = (Pipe *)vf;
        p->close();
        delete p;
    }
    else
    {
        auto f = (File *)vf;
        f->close();
        delete f;
    }

    return 0;
}

int fosFileTruncate(void *file, off64_t offset)
{
    File *f = (File *)(((FosFDEntry *)file)->private_data);
    assert(f && f->isFile());
    return f->truncate(offset);
}

int fosFileDescriptorCopy(void **vp, fd_t fd, InodeType inode)
{
    *vp = S_ISDIR(inode.type) || S_ISBLK(inode.type) ? (void *)new Dir(inode, fd)
                        : S_ISFIFO(inode.type) ? (void *)new Pipe(inode, fd)
                        : (void *)new File(inode, fd);

    return *vp ? OK : NO_MEMORY;
}

bool fosIsDir(void *vp)
{
    return ((VFile *)vp)->isDir();
}

bool fosIsPipe(void *vp)
{
    return ((VFile *)vp)->isPipe();
}

bool fosIsFile(void *vp)
{
    return ((VFile *)vp)->isFile();
}

InodeType fosFileIno(void *vp)
{
    return ((VFile *)vp)->inode();
}

file_id_t fosFileFd(void *vp)
{
    return ((VFile *)vp)->fd();
}

FosFileOperations fosFileOps =
{
    fosFileRead,
    fosFileWrite,
    fosFileClose,
    fosFilePoll,
    fosFileLseek,
    NULL,
    NULL,
};
