#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <system/status.h>
#include <utilities/debug.h>

int _statusToErrno(FosStatus status)
{
    switch(status)
    {
        case FOS_MAILBOX_STATUS_OK:
            return 0;
        case FOS_MAILBOX_STATUS_ALLOCATION_ERROR:
            return -ENOMEM;
        case FOS_MAILBOX_STATUS_EMPTY_ERROR:
            return EWOULDBLOCK;
        case FOS_MAILBOX_STATUS_NOT_FOUND_ERROR:
        case FOS_MAILBOX_STATUS_NOENT_ERROR:
            return -ENOENT;
        case FOS_MAILBOX_STATUS_NOT_DIR_ERROR:
            return -ENOTDIR;
        case FOS_MAILBOX_STATUS_NOT_EMPTY_ERROR:
            return -ENOTEMPTY;
        case FOS_MAILBOX_STATUS_EXIST_ERROR:
            return -EEXIST;
        case FOS_MAILBOX_STATUS_IS_DIR_ERROR:
            return -EISDIR;
        default:
            fprintf(stderr, "%s:%s:%d: The status here is %d.\n", __FILE__, __FUNCTION__, __LINE__, status);
            print_trace();
            assert(false);
            return -1;
    }
}

int statusToErrno(FosStatus status)
{
    int _errno = _statusToErrno(status);
    if(_errno != 0)
    {
        errno = -1 * _errno;
        return -1;
    }
    return 0;
}
