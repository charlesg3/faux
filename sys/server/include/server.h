#pragma once
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <fcntl.h>

#include <config/config.h>

#define TRACE_SERVER_CALLS 1
#define TRACE_CLIENT_CALLS 1

#define T(...)                                                         \
    do { char __buffer[4096];                                            \
        snprintf(__buffer,sizeof(__buffer),__VA_ARGS__);                \
        fprintf(stderr, "[%d] %-14s %10s:%-4d %s\n", getpid(), __FUNCTION__, strrchr(__FILE__, '/') + 1, __LINE__, __buffer); \
    } while (0)

#if TRACE_SERVER_CALLS
#define RT(dbg,...) do { if(ENABLE_DBG_SERVER_ALL || ENABLE_DBG_SERVER_##dbg ) T(__VA_ARGS__); } while (0)
#else
#define RT(...) do { } while (0)
#endif

#if TRACE_CLIENT_CALLS
#define LT(dbg,...) do { if(ENABLE_DBG_CLIENT_ALL || ENABLE_DBG_CLIENT_##dbg ) T(__VA_ARGS__); } while (0)
#else
#define LT(...) do { } while (0)
#endif

enum {
    OK        = 0,
    NOT_FOUND = -ENOENT,
    NO_MEMORY = -ENOMEM,
    EXIST     = -EEXIST,
    NOT_DIR   = -ENOTDIR,
    ISDIR     = -EISDIR,
    INVALID   = -EINVAL,
    NOT_EMPTY = -ENOTEMPTY,
    NO_SPACE  = -ENOSPC,
    ISSYM     = NOT_EMPTY // since not_empty isn't used in create call
};


extern int g_process;
extern int g_socket;
extern int g_sockets;
extern int g_servers_per_socket;
extern int g_socket_index;
extern int g_server_in_socket;
