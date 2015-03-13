#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <utilities/debug.h>
#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

extern FosFileOperations fosFileOps;

int __pipe(int pipefd[2])
{
    int retval = real_pipe(pipefd);

    if(retval == 0)
    {
        pipefd[0] = fdAddExternal(pipefd[0]);
        pipefd[1] = fdAddExternal(pipefd[1]);
    }

    return retval;
}

int __pipe2(int pipefd[2], int flags)
{
    int retval = 0;

    // Shared pipe managed by hare, abuse of O_EXCL flag
    if(flags & O_EXCL)
    {
        // create a pipe with the server
        void* private_data_read = NULL;
        void* private_data_write = NULL;
        retval = fosPipeOpen(&private_data_read, &private_data_write);
        if (retval < 0)
            goto exit_with_error;
        assert(private_data_read != NULL);
        assert(private_data_write != NULL);

        // assign an internal fd to the read end
        FosFDEntry* file_read = NULL;
        int internal_fd_read = fdCreate(&fosFileOps, private_data_read, 0, &file_read);
        if (internal_fd_read < 0) {
            retval = -ENOMEM;
            goto exit_with_error;
        }
        assert(file_read != NULL);

        // assign an internal fd to the write end
        FosFDEntry* file_write = NULL;
        int internal_fd_write = fdCreate(&fosFileOps, private_data_write, 0, &file_write);
        if (internal_fd_write < 0) {
            retval = -ENOMEM;
            goto exit_with_error;
        }
        assert(file_write != NULL);

        pipefd[0] = fdAddInternal(internal_fd_read);
        pipefd[1] = fdAddInternal(internal_fd_write);
    } else {
        retval = real_pipe2(pipefd, flags);

        if(retval == 0)
        {
            pipefd[0] = fdAddExternal(pipefd[0]);
            pipefd[1] = fdAddExternal(pipefd[1]);
        }
    }

exit_with_error:
    return retval;
}
