#include <stdio.h>
#include <assert.h>

#include <sys/fs/lib.h>
#include <sys/fs/interface.h>
#include <messaging/dispatch.h>
#include <utilities/debug.h>

#include "vfile.hpp"
#include "file.hpp"
#include <fd/fd.h>

using namespace fos;
using namespace fos::fs;
using namespace fos::fs::app;

extern bool g_cwd_in;

static char *g_excludes [] = {"/bin", "/dev", "/etc", "/home",
    "/lib", "/lib64", "/opt", "/proc", "/root", "/run",
    "/sbin", "/srv", "/sys", "/usr", "//usr", 0};

bool fosPathExcluded(const char *path)
{
    char *p;
    int i = 0;
    if(!g_cwd_in) return true;
    if(path[0] != '/') return false;
    for (p = g_excludes[0]; p; p = g_excludes[++i])
        if (strncmp(path, p, strlen(p)) == 0)
            return true;
    return false;
}

void fsCloneFds()
{
    int i;
    for(i = 0; i < FD_COUNT; i++)
    {
        if(fdIsInternal(i))
        {
            FosFDEntry *entry = fdGet(fdInt(i));
            assert(entry);
            VFile* vfile = (VFile *)entry->private_data;

            // Cloning the FD invalidates
            if(vfile->isFile())
                ((File *)vfile)->state(CACHE_INVALID);

            int ret = fosFileDup(vfile->inode().server, vfile->fd());
            assert(ret == OK);
        }
    }
}

void fsCloseFds()
{
    static bool closed_fds = false;
    if(!closed_fds)
    {
        int i;
        for(i = 0; i < FD_COUNT; i++)
        {
            if(fdIsInternal(i))
                close(i);
        }
        closed_fds = true;
    }
}

void fsFlattenFdInternal(int fd)
{
    FosFDEntry *file = fdGet(fdInt(fd));
    assert(file);
    if(file->num_fd > 1)
    {
        int ret = fosFileDup(fosFileIno(file->private_data).server, fosFileFd(file->private_data));
        assert(ret == OK);

        void* private_data = NULL;
        if(fosIsDir(file->private_data))
            fosDirAlloc(&private_data, fosFileIno(file->private_data), fosFileFd(file->private_data));
        else if(fosIsPipe(file->private_data))
            fosPipeAlloc(&private_data, fosFileIno(file->private_data), fosFileFd(file->private_data));
        else
            fosFileAlloc(&private_data, fosFileIno(file->private_data), fosFileFd(file->private_data));

        fdFlatten(fdInt(fd), file, private_data);
    }
}


void fsFlattenFds()
{
    int i;
    for(i = 0; i < FD_COUNT; i++)
    {
        if(fdIsInternal(i))
        {
            fsFlattenFdInternal(i);
        }
    }
}

extern Channel *g_fs_chan[CONFIG_SERVER_COUNT];

void fsReset(void)
{
    for(int i = 0; i < CONFIG_SERVER_COUNT; i++)
        g_fs_chan[i] = NULL;
}
