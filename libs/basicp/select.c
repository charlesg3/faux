//Posix Select Interface
//Author: Charles Gruenwald III
//Date: July 2010
#define _GNU_SOURCE

#include <sys/select.h>

/* According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <fd/fd.h>
#include <utilities/time_arith.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>

#define	FD_COPY(f, t)	(void)(*(t) = *(f))

//FIXME: handle other types of fds
//FIXME: return error
int __select(int nfds, fd_set *readfds, fd_set *writefds,
        fd_set *exceptfds, struct timeval *timeout)
{
    int fd;
    struct timeval start;
    struct timeval now;
    struct timeval end;

    if(timeout)
    {
        gettimeofday(&start, NULL);
        timeval_add(&end, &start, timeout);
    }

    bool timed_out = false;

    int count = 0;
    fd_set readable_fds;
    FD_ZERO(&readable_fds);
    while(!timed_out && !count)
    {
        for(fd = 0; fd < nfds; fd++)
        {
            if(FD_ISSET(fd, readfds))
            {
                FosFDEntry *file = fdGet(fd);
                if (file == NULL)
                {
                    if(!fdIsExternal(fd))
                    {
                        errno = EBADF;
                        return -1;
                    }

                    fd_set readers;
                    FD_ZERO(&readers);
                    FD_SET(fdExt(fd), &readers);
                    struct timeval internal_timeout;
                    internal_timeout.tv_sec = 0;
                    internal_timeout.tv_usec = 0;
                    int max_fd = fdExt(fd) + 1;
                    int fd_count = select(max_fd, &readers, NULL, NULL, &internal_timeout);
                    if(fd_count)
                    {
                        FD_SET(fd, &readable_fds);
                        count++;
                    }
                } // if
                else if ( file->file_ops->poll != NULL && file->file_ops->poll(file) )
                {
                    FD_SET(fd, &readable_fds);
                    count++;
                } // if
                else if ( file->file_ops->poll == NULL)
                    fprintf(stderr, "no poll found?\n");
            }
        }
        if(timeout) // see if a timeout was specified
        {
            gettimeofday(&now, NULL); // get the current time
            if(timeval_subtract(NULL, &end, &now)) // see if end time is past current time
                timed_out = true;
        }
    }

    if(readfds)
        FD_ZERO(readfds);
    FD_COPY(&readable_fds, readfds);
    if(writefds)
        FD_ZERO(writefds);
    if(exceptfds)
        FD_ZERO(exceptfds);

    return count;
}

