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
#include <stdbool.h>
#include <assert.h>

#include <config/config.h>
#include <sys/server/server.h>
#include <utilities/debug.h>

typedef uint64_t file_id_t;
typedef uint64_t fd_t;

typedef struct
{
    int server;
    file_id_t id;
} Inode;


#define InodeNew(server__,id__) ({ Inode __n; __n.server = server__; __n.id = id__; __n; })

#define InodeOnly(inodetype__) (*(Inode *)&inodetype__)

#define FS_NAME 1234
#define MAX_FNAME_LEN CONFIG_MAX_FNAME_LEN
#define FS_CHUNK_SIZE CONFIG_FS_CHUNK_SIZE
#define DIRECTORY_CACHE  ENABLE_DIRECTORY_CACHE

#define DIRECTORY_DISTRIBUTION ENABLE_DIRECTORY_DISTRIBUTION

#define DBG_SHOW_OPS ENABLE_DBG_SHOW_OPS
#define DBG_SHOW_FDS ENABLE_DBG_SHOW_FDS
#define DBG_SHOW_BUFFERCACHE ENABLE_DBG_SHOW_BUFFERCACHE
#define DBG_SHOW_ORPHANS ENABLE_DBG_SHOW_ORPHANS
#define DBG_SHOW_QUEUE_DELAY ENABLE_DBG_SHOW_QUEUE_DELAY
#define BUFFER_CACHE_LOG ENABLE_DBG_CLIENT_BUFFER_CACHE_LOG

enum {
    CACHE_MODIFIED,
    CACHE_EXCLUSIVE,
    CACHE_SHARED,
    CACHE_INVALID
};

enum {
    RMDIR_TRY,
    RMDIR_FAIL,
    RMDIR_SUCCESS,
    RMDIR_FORCE,
    RMDIR_AND_MAP
};

#ifdef __cplusplus
extern "C" {
#endif

enum FileType
{
    TYPE_FILE = S_IFREG,
    TYPE_DIR = S_IFDIR,
    TYPE_SYMLINK = S_IFLNK,
    TYPE_PIPE = S_IFIFO,
    TYPE_DIR_SHARED = S_IFBLK
};

typedef struct
{
    int server;
    file_id_t id;
    enum FileType type;
} InodeType;


#ifdef __cplusplus
}
#endif

#define S_ISSHR S_ISVTX
#define O_SYMLINK	04000000	/* for creating a symlink */

// reserve high 5 bits of the inode for server id to make inodes GUID
#define INODE_MASK 0x3FFFFFF
#define INODE_SHIFT 26

#include <stdio.h>

static inline unsigned int getCRC(unsigned int init, const char *data, unsigned int length)
{
    unsigned int i;
    int chk = init ? init : 0x12395679;

    for (i = 0; i < length; i++)
        chk += ((int)(data[i]) * (i + 1));

    return chk;
}

extern int g_dest_server;
static inline unsigned int fosPathHashn(file_id_t id, const char *path, int pathlen)
{
    unsigned int hash = getCRC(0, (char *)&id, sizeof(id));
    hash = getCRC(hash, path, pathlen);
    return hash;
}

// This function is used when we aren't distributing directories, thus
// the id will be set to zero, but we still mod for the number of
// servers.
static inline unsigned int fosPathHashDirect(const char *path)
{
    return fosPathHashn(0, path, strlen(path)) % CONFIG_FS_SERVER_COUNT;
}

static inline int fosPathOwnern(InodeType inode, const char *path, int pathlen)
{
    if(inode.id == 0)
    {
        if(pathlen == 0) return 0;
        if(pathlen == 2 && path[0] == '.' && path[1] == '.') return 0;
    }

    if(inode.type == TYPE_DIR_SHARED)
        return CONFIG_FS_SERVER_COUNT > 1 ? fosPathHashn(inode.id, path, pathlen) % CONFIG_FS_SERVER_COUNT : 0;
    else if(inode.type == TYPE_DIR)
        return inode.server;
    else
    {
        fprintf(stderr, "unknown inode type: %d on path: %s\n", inode.type, path);
        print_trace();
        assert(false);
    }
}

static inline int fosPathOwner(InodeType inode, const char *path)
{
    return CONFIG_FS_SERVER_COUNT > 1 ?  fosPathOwnern(inode, path, strlen(path)) : 0;
}

