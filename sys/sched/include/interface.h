#pragma once
#pragma GCC diagnostic ignored "-pedantic"

#include <sys/fs/interface.h>
#include <sys/types.h>
#include <sys/server/server.h>

enum SchedMessageTypes
{
    EXEC = 256,
    NUM_SCHED_TYPES
};

enum {
    CHILD_EXIT,
    EXEC_ERROR
};

struct ExecRequest
{
    int pid;
    int envp_offset;
    char filename[0];
} __attribute__ ((packed));

struct ExecReply
{
    int type;
    int retval;
    int errno_reply;
} __attribute__ ((packed));
