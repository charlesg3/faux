#include <cassert>
#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <dirent.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <utime.h>

#include <sys/sched/lib.h>
#include <sys/sched/interface.h>
#include <sys/fs/lib.h>
#include <sys/fs/interface.h>
#include <sys/fs/local_ops.h>
#include <sys/fs/buffer_cache_log.h>
#include <utilities/tsc.h>
#include <utilities/debug.h>
#include <name/name.h>
#include <messaging/dispatch.h>
#include <messaging/channel_interface.h>

#include "pipe.hpp"
#include "file.hpp"
#include "dir.hpp"
#include "dir_cache.hpp"

#define ROOT_INO 0

#define ALIGN(x, a)            ALIGN_MASK(x, (typeof(x))(a) - 1)
#define ALIGN_MASK(x, mask)    (((x) + (mask)) & ~(mask))

using namespace std;
using namespace fos;
using namespace fs;
using namespace app;

DispatchTransaction fosDirentReq(InodeType inode, off64_t offset, uint64_t size);
int fosDirentWaitReply(DispatchTransaction trans, uint64_t *size, bool *more, void (*_callback)(void *, uint64_t, void *), void *p);

// used for creation affinity
extern int g_socket_map[40];

static DirCache g_dir_cache;
void fosDirCacheClear()
{
    g_dir_cache.clear();
}

// Channel management
Address g_fs_addr[CONFIG_SERVER_COUNT];
Channel * g_fs_chan[CONFIG_SERVER_COUNT];

Channel *establish_fs_chan(int i)
{
    assert(i < CONFIG_SERVER_COUNT);

    if(!g_fs_chan[i])
    {
        if(!g_fs_addr[i])
            while(!name_lookup(FS_NAME + i, &g_fs_addr[i]))
                ;
        g_fs_chan[i] = dispatchOpen(g_fs_addr[i]);
    }
    return g_fs_chan[i];
}

// Server RPC Calls
int fosCreate(int server, InodeType parent, const char *path, int flags, mode_t mode, InodeType *exist)
{
    int retval;
    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(server);

    // the +1 for null termination
    size_t request_size = sizeof(CreateRequest) + strlen(path) + 1;
    CreateRequest* request = (CreateRequest *) dispatchAllocate(chan, request_size);

    request->parent = parent;
    request->flags = flags;
    request->mode = mode;
    strcpy(request->path, path);

    // root node already exists...
    if(parent.id == ROOT_INO && path[0] == '/' && path[1] == '\0') return OK;

    DispatchTransaction transaction = dispatchRequest(chan, CREATE, request, request_size);
    hare_wake(server);

    StatusInodeTypeReply *reply;
    size_t size = dispatchReceive(transaction, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(*reply));
    retval = reply->retval;
    *exist = reply->inode;

    dispatchFree(transaction, reply);

    LT(CREATE, "%llx@%d:%s (%o) @%d -> %llx@%d (rc: %d)", parent.id, parent.server, path, mode, server, exist->id, exist->server, retval);

    return retval;
}

int fosDestroy(int server, file_id_t id)
{
    int retval;
    Channel *chan = establish_fs_chan(server);

    IdRequest *request = (IdRequest *)dispatchAllocate(chan, sizeof(*request));

    request->id = id;

    DispatchTransaction transaction = dispatchRequest(chan, DESTROY, request, sizeof(*request));
    hare_wake(server);

    StatusReply *reply;
    size_t size = dispatchReceive(transaction, (void **) &reply);
    assert(size == sizeof(*reply));
    retval = reply->retval;
    dispatchFree(transaction, reply);

    return retval;
}

int fosAddMap(int server, InodeType parent, const char *path, InodeType child, bool overwrite, InodeType *exist)
{
    OP_COUNTER(ADDMAP);
    int retval;
    AddMapRequest *req;
    DispatchTransaction trans;
    StatusInodeTypeReply *reply;
    __attribute__((unused)) size_t size;
    Channel *chan = NULL;
    int pathlen = 0;
    pathlen = strlen(path) + 1;

    EVENT_START(RPC);
    chan = establish_fs_chan(server);

    // add the mapping
    req = (AddMapRequest *) dispatchAllocate(chan, sizeof(*req) + pathlen);

    req->parent = InodeOnly(parent);
    req->child = child;
    req->overwrite = overwrite;
    req->update = false;
    strcpy(req->path, path);

    trans = dispatchRequest(chan, ADDMAP, req, sizeof(*req) + pathlen);
    hare_wake(server);

    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(*reply));

    retval = reply->retval;
    if(exist) *exist = reply->inode;

    dispatchFree(trans, reply);

    if(DIRECTORY_CACHE && (retval == OK || overwrite))
        DirCache::dc().add(parent, path, strlen(path), child);

    return retval;
}

int fosUpdateMap(int server, InodeType parent, const char *path, InodeType child, bool overwrite, InodeType *exist, InodeType old_parent, const char *old_path)
{
    OP_COUNTER(ADDMAP);
    int retval;
    UpdateMapRequest *req;
    DispatchTransaction trans;
    StatusInodeTypeReply *reply;
    __attribute__((unused)) size_t size;
    Channel *chan = NULL;
    int pathlen = 0;
    int full_pathlen;
    pathlen = strlen(path) + 1;
    full_pathlen = pathlen + strlen(old_path) + 1;

    EVENT_START(RPC);
    chan = establish_fs_chan(server);

    // add the mapping
    req = (UpdateMapRequest *) dispatchAllocate(chan, sizeof(*req) + full_pathlen);

    req->parent = InodeOnly(parent);
    req->child = child;
    req->overwrite = overwrite;
    req->update = true;
    req->old_parent = old_parent;
    strcpy(req->path, path);
    strcpy(req->path + pathlen, old_path);

    trans = dispatchRequest(chan, ADDMAP, req, sizeof(*req) + full_pathlen);
    hare_wake(server);

    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(*reply));

    retval = reply->retval;
    if(exist) *exist = reply->inode;

    dispatchFree(trans, reply);

    if(DIRECTORY_CACHE && (retval == OK || overwrite))
        DirCache::dc().add(parent, path, strlen(path), child);

    return retval;
}

int fosRemoveMap(int server, InodeType parent, const char *path)
{
    OP_COUNTER(REMOVEMAP);
    int retval;
    IdPathRequest *req;
    DispatchTransaction trans;
    StatusReply *reply;
    __attribute__((unused)) size_t size;
    Channel *chan = NULL;
    int pathlen = 0;

    EVENT_START(RPC);
    chan = establish_fs_chan(server);

    pathlen = strlen(path) + 1;

    // remove the mapping
    req = (IdPathRequest *) dispatchAllocate(chan, sizeof(*req) + pathlen);

    req->id = parent.id;
    strcpy(req->path, path);

    trans = dispatchRequest(chan, REMOVEMAP, req, sizeof(*req) + pathlen);
    hare_wake(server);

    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(StatusReply));

    retval = reply->retval;

    dispatchFree(trans, reply);
    return retval;
}

static int _fosOpen(InodeType inode, int flags, fd_t *fd, int *type, size_t *filesize, int *state)
{
    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(inode.server);
    OpenRequest *request = (OpenRequest *)dispatchAllocate(chan, sizeof(*request));
    int retval;
    request->id = inode.id;
    request->flags = flags;
    DispatchTransaction transaction = dispatchRequest(chan, OPEN, request, sizeof(*request));
    hare_wake(inode.server);

    OpenReply *reply;
    size_t size = dispatchReceive(transaction, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(OpenReply));

    retval = reply->retval;
    *fd = reply->fd;
    *filesize = reply->size;
    *state = reply->state;
    *type = reply->type;

    dispatchFree(transaction, reply);

    return retval;
}

static int _fosCreateOpen(int server, InodeType parent, const char *path, int flags, int mode, fd_t *fd, int *type, size_t *filesize, int *state, InodeType *exist)
{
    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(server);
    int reqsize = sizeof(CreateOpenRequest) + strlen(path) + 1;
    int retval;
    CreateOpenRequest *request = (CreateOpenRequest *)dispatchAllocate(chan, reqsize);
    request->parent = InodeOnly(parent);
    request->flags = flags;
    request->mode = mode;
    strcpy(request->path, path);
    DispatchTransaction transaction = dispatchRequest(chan, OPEN, request, reqsize);
    hare_wake(server);

    OpenReply *reply;
    size_t size = dispatchReceive(transaction, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(OpenReply));

    retval = reply->retval;
    *fd = reply->fd;
    *type = reply->type;
    *filesize = reply->size;
    *state = reply->state;
    *exist = reply->exist;

    dispatchFree(transaction, reply);

    return retval;
}

int fosPipeOpen(void** vp_read, void **vp_write)
{
    int server = 0;
    fd_t read_fd;
    fd_t write_fd;
    InodeType inode;

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(server);
    int reqsize = sizeof(PipeRequest);
    int retval;
    PipeRequest *request = (PipeRequest *)dispatchAllocate(chan, reqsize);
    DispatchTransaction transaction = dispatchRequest(chan, PIPE, request, reqsize);
    hare_wake(server);

    PipeReply *reply;
    size_t size = dispatchReceive(transaction, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(*reply));

    retval = reply->retval;
    read_fd = reply->read_fd;
    write_fd = reply->write_fd;
    inode.server = reply->inode.server;
    inode.id = reply->inode.id;
    inode.type = TYPE_PIPE;

    dispatchFree(transaction, reply);

    if(retval != OK) goto exit;

    *vp_read = (void *)new Pipe(inode, read_fd);
    *vp_write = (void *)new Pipe(inode, write_fd);

exit:
    return retval;
}

int fosFileOpen(void** vp, InodeType parent, const char *path, int flags, mode_t mode)
{
    OP_COUNTER(OPEN);
    int retval;

    bool cached = false;

    InodeType inode;

    // Save away these variables as the fosLookupParent function
    // can and will modify them.
    //
    // Later, if the open fails, we need to be able to clear the
    // cache and re-try in case the open is simply failing due
    // to a cache inconsistency.
    InodeType save_parent = parent;
    const char *save_path = path;

    // first see if we need to create the file...
    if(flags & O_CREAT)
    {
        // directories must be created using mkdir
        // though we will internally use this flag
        // to overload teh create function
        if(flags & O_DIRECTORY) return INVALID;

        InodeType result;
        retval = fosLookupParent(parent, &path, &result, &cached);
        if(retval != OK) return retval;

        parent = result;

        int dir_entry_server = fosPathOwner(parent, path);

        InodeType exist;

        // If the parent is a shared directory then the path owner will be
        // distributed and therefore home the inode there. If the directory
        // isn't distributed, then choose a random server based on the path
        inode.server = parent.type == TYPE_DIR_SHARED ? dir_entry_server : fosPathHashDirect(path);

        // if we're only writing it then we don't care that much about affinity.
        if(ENABLE_CREATION_AFFINITY && ! (flags & O_WRONLY))
        {
            if(g_socket_map[dir_entry_server] != g_socket_map[g_dest_server])
                inode.server = g_dest_server;
        }

        bool exists;

        // if the inode is going to live on the directory server, then try to
        // do the create/addmap/open call all in one below rather than separate calls.
        if(inode.server == dir_entry_server) goto retry;

        retval = fosCreate(inode.server, parent, path, flags, mode, &exist);
        if(retval == EXIST && flags & O_EXCL) return retval;

        exists = (retval == EXIST);

        // It's okay if it alreay exists as long as we weren't
        // trying to create it exlusively
        if(retval == EXIST) retval = OK;

        if(retval == ISSYM) assert(false);
        //FIXME: if we call create on a symlink, then we need to:
        //1. call readlink to get the dest
        //2. call lookup and then create on that dest
        if(retval != OK) return retval;

        // If it doesn't already exist at the home node
        // then we potentially created a new entry, but we
        // have to update the directory entry mapping to
        // ensure that it persists. If that fails, then
        // we have to remove it and find out what exists there.
        if(!exists)
        {
            // the create call returns the new inode's id in the exist output parameter.
            // if the inode didn't exist, then we just created a new one and need to copy
            // that id.
            inode.id = exist.id;
            inode.type = TYPE_FILE;

            bool overwrite = false;
            retval = fosAddMap(dir_entry_server, parent, path, inode, overwrite, &exist);

            // exists after trying to add map, must be on different server...
            // we'll remove what we created and lookup existing entry.
            if(retval == EXIST)
            {
                retval = fosDestroy(inode.server, inode.id);
                assert(retval == OK);
            }
        }

        inode = exist;

        // file has now been created, remove the CREAT flag for
        // further open calls (below).
        flags &= ~O_CREAT;
    }
    else
    {
        retval = fosLookupParent(parent, &path, &parent, NULL);
        if(retval != OK) return retval;

        retval = fosLookup(parent, path, &inode, &cached);
        LT(OPEN, "looking up %llx:%s -> %llx@%d", parent.id, path, inode.id, inode.server);
    }
retry:

    LT(OPEN, "trying to open %llx@%d", inode.id, inode.server);

    // The root node doesn't actually have a parent so when the client tries to
    // look up parent = 0 and path = "/", just use the "." for the path since
    // "." is a child of 0 that will return /
    if(parent.id == ROOT_INO && path[0] == '/' && path[1] == '\0')
        assert(false); // this should have already resolved 

    if(retval != OK) return retval;

    // when create is called above a 3-way create occurs by first creating the inode
    // then adding the mapping to the directory server and finally opening the file.
    // The first two steps may be performed above. In this case, the open call below
    // simply needs to open a file descriptor pointing to an inode at this server.
    //
    // In the situation where the inode will live at the directory entry server, then
    // the three calls can be made in one (saving a few rounde trips). In this situation
    // the create Open call is called with the create flag still in place (removed
    // above if the 3-way function was used). Thus we also need to pass in the path
    // for the create / add_map functions.

    // create not called above, must be trying create + add_map + open in one call...
    // in this case we pass the path as well.

    fd_t fd;
    size_t filesize;
    int state;
    int type;

    if(flags & O_CREAT)
    {
        InodeType exist;
        retval = _fosCreateOpen(inode.server, parent, path, flags, mode, &fd, &type, &filesize, &state, &exist);

        if(retval == EXIST) 
        {
            if(flags & O_EXCL) return retval;

            // clear the creat flag since the file has now been created (or
            // already was created)
            flags &= ~ O_CREAT;

            retval = OK;
            // Inode already exists on the server we are contacting, therefore
            // the file descriptor has been opened for us and is valid.
            // Otherwise the file exists on a separate server and we must
            // contact that server to open the file.
            if(exist.server != inode.server)
                retval = _fosOpen(exist, flags, &fd, &type, &filesize, &state);
        }

        inode = exist;
    }
    else
    {
        retval = _fosOpen(inode, flags, &fd, &type, &filesize, &state);
    }

    inode.type = (FileType)type;

    if(retval != OK) { LT(OPEN, "open error: %d (inode: %llx) on %s @ %d", retval, inode.id, path, inode.server); }

    // if we try to open an existing file and get NOT_FOUND back, it could just
    // be that the cache was stale. Clear the cache and try again (once)
    if(DIRECTORY_CACHE && cached && !(flags & O_CREAT) && retval == NOT_FOUND)
    {
        _fosClearCache(save_parent, save_path, strlen(save_path));
        retval = fosLookup(save_parent, save_path, &inode, &cached);
        cached = false;
        goto retry;
    }

    if(retval != OK) goto exit;

    if (S_ISREG(inode.type)) {
        fosNumaSched(inode, filesize);
        File *f_p = new File(inode, fd);
        f_p->size(filesize);
        f_p->cache_blocks();
        f_p->state(state);
        if(flags & O_APPEND)
            f_p->offset(filesize);
        *vp = (void*)f_p;
    } else if (S_ISDIR(inode.type) || S_ISBLK(inode.type)) {
        *vp = (void*) new Dir(inode, fd);
    } else {
        // FIXME: wtf?!
        assert(false);
    }

    if(DIRECTORY_CACHE && (flags & O_CREAT))
        DirCache::dc().add(parent, path, strlen(path), inode);
exit:
    return retval;
}

void fosFileAlloc(void** vp, InodeType inode, fd_t fd)
{
    *vp = (void*) new File(inode, fd);
}

void fosDirAlloc(void** vp, InodeType inode, fd_t fd)
{
    *vp = (void*) new Dir(inode, fd);
}

void fosPipeAlloc(void** vp, InodeType inode, fd_t fd)
{
    *vp = (void*) new Pipe(inode, fd);
}

int fosUnlink(InodeType parent, const char *path)
{
    OP_COUNTER(UNLINK);
    LT(UNLINK, "%llx:%s", parent.id, path);
    int retval;
    UnlinkRequest *req;
    DispatchTransaction trans;
    StatusReply *reply;
    __attribute__((unused)) size_t size;
    int server = 0;

    if((retval = fosLookupParent(parent, &path, &parent, NULL)) != OK)
        return retval;

    int dir_entry_server = fosPathOwner(parent, path);

    InodeType child;

    if((retval = fosLookup(parent, path, &child, NULL)) != OK)
        return retval;

    // if the dir entry is on a different server then remove
    // that mapping.
    int pathlen = 0;
    if(child.server != dir_entry_server)
        retval = fosRemoveMap(dir_entry_server, parent, path);
    else
        pathlen = strlen(path) + 1;

    assert(retval == OK);

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(child.server);

    req = (UnlinkRequest *) dispatchAllocate(chan, sizeof(*req) + pathlen);
    req->id = child.id;
    req->parent_id = UINT64_MAX;

    if(child.server == dir_entry_server)
    {
        req->parent_id = parent.id;
        strcpy(req->path, path);
    }

    trans = dispatchRequest(chan, UNLINK, req, sizeof(*req) + pathlen);
    hare_wake(server);
    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(StatusReply));
    retval = reply->retval;
    dispatchFree(trans, reply);

    if (DIRECTORY_CACHE && retval == OK)
        DirCache::dc().remove(parent, path);

    fosBufferCacheLog(BCL_DELETE, child.id, 0);

    return retval;
}

static DispatchTransaction _RmdirSendReq(int server, file_id_t id, int phase)
{
    RmdirRequest *req;
    Channel *chan = establish_fs_chan(server);
    req = (RmdirRequest *) dispatchAllocate(chan, sizeof(*req));
    req->id = id;
    req->phase = phase;
    DispatchTransaction ret = dispatchRequest(chan, RMDIR, req, sizeof(*req));
    hare_wake(server);
    return ret;
}

static int _RmdirWaitReply(DispatchTransaction trans)
{
    StatusReply *reply;
    int retval;
    __attribute__((unused)) size_t size;
    size = dispatchReceive(trans, (void **) &reply);
    assert(size == sizeof(StatusReply));
    retval = reply->retval;
    dispatchFree(trans, reply);
    return retval;
}

int fosRmdirAndMap(int server, file_id_t id, InodeType parent, char *path)
{
    RmdirAndMapRequest *req;
    StatusReply *reply;
    int retval;
    int pathlen = strlen(path) + 1;

    Channel *chan = establish_fs_chan(server);
    req = (RmdirAndMapRequest *) dispatchAllocate(chan, sizeof(*req) + pathlen);
    req->id = id;
    req->phase = RMDIR_AND_MAP;
    req->parent = parent;
    strcpy(req->path, path);
    DispatchTransaction trans = dispatchRequest(chan, RMDIR, req, sizeof(*req) + pathlen);
    hare_wake(server);

    __attribute__((unused)) size_t size;
    size = dispatchReceive(trans, (void **) &reply);
    assert(size == sizeof(StatusReply));
    retval = reply->retval;
    dispatchFree(trans, reply);
    return retval;
}

int fosDoRmdir(int server, file_id_t inode, int phase)
{
    return _RmdirWaitReply(_RmdirSendReq(server, inode, phase));
}

int fosRmdir(InodeType parent, const char *path)
{
    OP_COUNTER(RMDIR);
    LT(RMDIR, "%llx:%s", parent.id, path);
    int retval = OK;
    int action;
    int contacted;

    // Lookup the parent while removing the final '/' for the lookup
    bool lastsep = path[strlen(path) - 1] == '/' ? true : false;
    //FIXME: some way to do this without modifying a char *path?
    // ignore slashes on the end of dirs that we are creating
    if(lastsep) ((char *)path)[strlen(path) - 1] = '\0';

    if((retval = fosLookupParent(parent, &path, &parent, NULL)) != OK)
        return retval;

    InodeType child;
    int dir_entry_server = fosPathOwner(parent, path);

    if((retval = fosLookup(parent, path, &child, NULL)) != OK)
        return retval;

    if(! (child.type == TYPE_DIR_SHARED))
    {
        // Coalesce if child inode and parent dir entry server are same
        if(dir_entry_server == child.server)
        {
            retval = fosRmdirAndMap(child.server, child.id, parent, path);
            if(retval != OK) return retval;
        } else {
            retval = fosDoRmdir(child.server, child.id, RMDIR_FORCE);
            if(retval != OK) return retval;

            retval = fosRemoveMap(dir_entry_server, parent, path);
            assert(retval == OK);
        }

        goto done_rmdir;
    }

    // PHASE 1: Try all servers, if any fails then go back and unlock
    unsigned int i;
    int64_t rm_retvals[CONFIG_FS_SERVER_COUNT];

        do {
            retval = fosDoRmdir(child.server, child.id, RMDIR_TRY);
        } while ( retval == -EAGAIN );
    rm_retvals[child.server] = retval;
    if(retval != OK) return retval;

#if ENABLE_RM_SCATTER_GATHER
    DispatchTransaction rmtrans[CONFIG_FS_SERVER_COUNT];
    for(i = 0; i < CONFIG_FS_SERVER_COUNT; i++)
    {
        if(i == child.server) continue;
        rmtrans[i] = _RmdirSendReq(i, child.id, RMDIR_TRY);
    }

    for(i = 0; i < CONFIG_FS_SERVER_COUNT; i++)
    {
        if(i == child.server) continue;
        rm_retvals[i] = _RmdirWaitReply(rmtrans[i]);
        if(rm_retvals[i] != OK) retval = rm_retvals[i];
    }

#else
    for(i = 0; i < CONFIG_FS_SERVER_COUNT; i++)
    {
        if(i == child.server) continue;
        do {
            retval = fosDoRmdir(i, child.id, RMDIR_TRY);
        } while ( retval == -EAGAIN );
        if(retval != OK) break;
    }
#endif

    // PHASE 2: Commit
    action = retval == OK ? RMDIR_SUCCESS : RMDIR_FAIL;
#if ENABLE_RM_SCATTER_GATHER
    for(i = 0; i < CONFIG_FS_SERVER_COUNT; i++)
    {
        if(rm_retvals[i] == NOT_EMPTY) continue;
        rmtrans[i] = _RmdirSendReq(i, child.id, action);
    }

    for(i = 0; i < CONFIG_FS_SERVER_COUNT; i++)
    {
        if(rm_retvals[i] == NOT_EMPTY) continue;
        int retval2 = _RmdirWaitReply(rmtrans[i]);
        assert(retval2 == OK);
    }
#else
    contacted = i;
    for(i = 0; i < contacted; i++)
    {
        int retval2 = fosDoRmdir(i, child.id, action);
        assert(retval2 == OK);
    }

    if(action == RMDIR_FAIL && contacted < child.server)
    {
        retval = fosDoRmdir(child.server, child.id, action);
        assert(retval == OK);
    }
#endif

    if(action != RMDIR_SUCCESS) return retval;

    // if everyone was able to remove it (or didn't have it
    // in the first place) then remove the mapping at the
    // directory entry

    retval = fosRemoveMap(dir_entry_server, parent, path);

done_rmdir:
    if(DIRECTORY_CACHE && retval == OK)
    {
        DirCache::dc().remove(parent, path);
        DirCache::dc().remove(child, "..");
    }

    if(lastsep) ((char *)path)[strlen(path)] = '/';
    return retval;
}

int fosMkdir(InodeType parent, const char *path, mode_t mode)
{
    OP_COUNTER(CREATE);
    LT(MKDIR, "%llx:%s (%o)", parent.id, path, mode);
    int retval;
    int server = 0;

    // can't make root dir (already exists)
    if(parent.id == 0 && path[0] == '/' && path[1] == '\0')
        return OK;

    if(strcmp(path, ".") == 0)
        return OK;

    // Lookup the parent while removing the final '/' for the lookup
    bool lastsep = path[strlen(path) - 1] == '/' ? true : false;
    //FIXME: some way to do this without modifying a char *path?
    // ignore slashes on the end of dirs that we are creating
    if(lastsep) ((char *)path)[strlen(path) - 1] = '\0';

    const char *savepath = path;
    int dir_entry_server;
    int inode_server;
    InodeType exist;
    InodeType child;
    int dot_server;
    int dot_dot_server;

    if((retval = fosLookupParent(parent, &path, &parent, NULL)) != OK)
        goto done_mkdir;

    if(parent.type == TYPE_DIR_SHARED)
    {
        inode_server = dir_entry_server = fosPathOwner(parent, path);
    } else {
        dir_entry_server = fosPathOwner(parent, path);
        inode_server = fosPathHashDirect(path);
        if(ENABLE_CREATION_AFFINITY)
            if(g_socket_map[inode_server] != g_socket_map[g_dest_server])
                inode_server = g_dest_server;
    }

    retval = fosCreate(inode_server, parent, path, O_DIRECTORY, mode, &child);
    if(retval != OK) goto done_mkdir;

    if(child.type == TYPE_DIR_SHARED)
    {
        dot_dot_server = fosPathOwner(child, "..");
        // mapping is added server side when we create the directory inode
        // so the mapping is added server side
        if(dot_dot_server != inode_server)
        {
            retval = fosAddMap(dot_dot_server, child, "..", parent, false, NULL);
            assert(retval == OK);
        }
    }

    // if the parent directory's server isn't the server where we're
    // placing the child then we need to add the mapping
    if(parent.type == TYPE_DIR)
    if(parent.server != inode_server)
    {
        assert(child.type == TYPE_DIR_SHARED || child.type == TYPE_DIR);
        retval = fosAddMap(parent.server, parent, path, child, false, &exist);
        if(retval != OK) 
        {
            fosDestroy(inode_server, child.id);
            goto done_mkdir;
        }
    }
    else
        DirCache::dc().add(parent, path, strlen(path), child);


done_mkdir:
    if(lastsep) ((char *)path)[strlen(path)] = '/';
    LT(MKDIR, "%llx:%s -> %d", parent.id, path, retval);
    return retval;
}

int fosChmod(InodeType parent, const char *path, mode_t mode)
{
    OP_COUNTER(CHMOD);
    int retval;
    ChmodRequest *req;
    DispatchTransaction trans;
    StatusReply *reply;
    __attribute__((unused)) size_t size;

    InodeType child;

    if((retval = fosLookup(parent, path, &child, NULL)) != OK)
        return retval;

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(child.server);

    int pathlen = path ? strlen(path) + 1 : 1;

    // Lookup the file to see if it already exists
    req = (ChmodRequest *) dispatchAllocate(chan, sizeof(*req) + pathlen);

    req->inode = child.id;
    req->mode = mode;

    trans = dispatchRequest(chan, CHMOD, req, sizeof(*req) + pathlen);
    hare_wake(child.server);

    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(StatusReply));

    retval = reply->retval;

    dispatchFree(trans, reply);

    return retval;
}


int fosChown(InodeType parent, const char *path, uid_t uid, gid_t gid)
{
    OP_COUNTER(CHOWN);
    int retval;
    ChownRequest *req;
    DispatchTransaction trans;
    StatusReply *reply;
    __attribute__((unused)) size_t size;

    InodeType child;

    if((retval = fosLookup(parent, path, &child, NULL)) != OK)
        return retval;

    int pathlen = path ? strlen(path) + 1 : 1;

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(child.server);

    // Lookup the file to see if it already exists
    req = (ChownRequest *) dispatchAllocate(chan, sizeof(*req) + pathlen);

    req->inode = child.id;
    req->uid = uid;
    req->gid = gid;

    trans = dispatchRequest(chan, CHOWN, req, sizeof(*req) + pathlen);
    hare_wake(child.server);

    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(StatusReply));

    retval = reply->retval;

    dispatchFree(trans, reply);

    return retval;
}

int fosTruncate(InodeType parent, const char *path, off64_t length)
{
    OP_COUNTER(TRUNC);
    int retval;
    TruncRequest *req;
    DispatchTransaction trans;
    StatusReply *reply;
    __attribute__((unused)) size_t size;

    InodeType child;

    // if no path is given then we're called from a file object
    // otherwise it is truncate on a path
    if((retval = fosLookupParent(parent, &path, &parent, NULL)) != OK)
        return retval;

    if((retval = fosLookup(parent, path, &child, NULL)) != OK)
        return retval;

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(child.server);

    // Lookup the file to see if it already exists
    req = (TruncRequest *) dispatchAllocate(chan, sizeof(*req));

    req->inode = child.id;
    req->len = length;

    trans = dispatchRequest(chan, TRUNC, req, sizeof(*req));
    hare_wake(child.server);

    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(StatusReply));

    retval = reply->retval;

    dispatchFree(trans, reply);

    // truncate the symlink's destination
    if(retval == ISSYM)
    {
        char link_dest[MAX_FNAME_LEN];
        fosDoReadlink(parent, link_dest, MAX_FNAME_LEN);
        return fosTruncate(parent, link_dest, length);
    }

    return retval;
}

int fosFStatAt64_(InodeType parent, const char *path, struct FauxStat64 *buf, bool deref)
{
    OP_COUNTER(FSTAT);
    int retval;
    IdRequest *req;
    DispatchTransaction trans;
    StatReply *reply;
    __attribute__((unused)) size_t size;

    InodeType child;

    bool cached;

    InodeType parent_save = parent;
    const char *path_save = path;

    if((retval = fosLookupParent(parent, &path, &parent, &cached)) != OK)
        return retval;

    if((retval = fosLookup(parent, path, &child, &cached)) != OK)
        return retval;

retry:

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(child.server);

    // Lookup the file to see if it already exists
    req = (IdRequest *) dispatchAllocate(chan, sizeof(*req));
    req->id = child.id;
    trans = dispatchRequest(chan, FSTAT, req, sizeof(*req));
    hare_wake(child.server);
    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(StatReply));
    retval = reply->retval;

    bool islink = S_ISLNK(reply->buf.st_mode);

    //fosAccess uses fstat but doesn't need the buffer so it passes NULL.
    //Access is rarely used and thus not optimized as a separate call.
    if(buf) memcpy(buf, &reply->buf, sizeof(*buf));

    if(buf && S_ISBLK(reply->buf.st_mode))
    {
        buf->st_mode &= ~S_IFMT;
        buf->st_mode |= S_IFDIR;
    }

    dispatchFree(trans, reply);

    if(unlikely(islink) && retval == OK && deref)
    {
        char link_dest[MAX_FNAME_LEN];
        fosDoReadlink(child, link_dest, MAX_FNAME_LEN);
        LT(FSTAT, "%llx:%s => (%llx) %d (symlink to: %s)", parent.id, path, child.id, retval, link_dest);

        //FIXME: symlinks pointing to themselves will infinite loop here
        return fosFStatAt64_(parent, link_dest, buf, false);
    }

    LT(FSTAT, "[%d] %llx:%s (%llx) => %d", child.server, parent.id, path, child.id, retval);
    if(DIRECTORY_CACHE && cached)
        if(retval != OK)
        {
            _fosClearCache(parent, path, strlen(path));
            parent = parent_save;
            path = path_save;
            if((retval = fosLookup(parent, path, &child, NULL)) != OK)
                return retval;
            cached = false;
            goto retry;
        }

    return retval;
}

int fosFStatAt64(InodeType parent, const char *path, struct stat64 *buf, bool deref)
{
    struct FauxStat64 buf64;
    int retval = fosFStatAt64_(parent, path, &buf64, deref);

    buf->st_dev = buf64.st_dev;
    buf->st_ino = buf64.st_ino;
    buf->st_mode = buf64.st_mode;
    buf->st_nlink = buf64.st_nlink;
    buf->st_uid = buf64.st_uid;
    buf->st_gid = buf64.st_gid;
    buf->st_rdev = buf64.st_rdev;
    buf->st_blocks = buf64.st_blocks;
    buf->st_size = buf64.st_size;
    buf->st_atim = buf64._st_atime;
    buf->st_mtim = buf64._st_mtime;
    buf->st_ctim = buf64._st_ctime;

    return retval;
}

int fosFStatAt(InodeType parent, const char *path, struct stat *buf, bool deref)
{
    struct FauxStat64 buf64;
    int retval = fosFStatAt64_(parent, path, &buf64, deref);

    buf->st_dev = buf64.st_dev;
    buf->st_ino = buf64.st_ino;
    buf->st_mode = buf64.st_mode;
    buf->st_nlink = buf64.st_nlink;
    buf->st_uid = buf64.st_uid;
    buf->st_gid = buf64.st_gid;
    buf->st_rdev = buf64.st_rdev;
    buf->st_blocks = buf64.st_blocks;
    buf->st_size = buf64.st_size;
    buf->st_atim = buf64._st_atime;
    buf->st_mtim = buf64._st_mtime;
    buf->st_ctim = buf64._st_ctime;

    return retval;
}

int fosAccess(InodeType parent, const char *path, int mode)
{
    //FIXME: this ignores mode
    return fosFStatAt64_(parent, path, NULL, true);
}

int fosFileLink(const char *oldpath, const char *newpath)
{
    //not implemented
    assert(false);
}

// Rename must do 2 things:
// 1. Update the mapping at the new location (the new parent directory must have an
// entry for the file -- only if the entry doesn't exist already)
//
// 2. Remove the mapping at the old location (the old parent directory must not have
// an entry for the file)
int fosRename(InodeType parent, const char *oldpath, const char *newpath)
{
    LT(RENAME, "%llx@%d:%s -> %s", parent.id, parent.server, oldpath, newpath);

    int retval;

    InodeType old_parent, new_parent, child, exist;

    int dot_dot_server;

    // First make sure we can find the parents...
    if(strstr(oldpath, "/"))
    {
        if((retval = fosLookupParent(parent, &oldpath, &old_parent, NULL)) != OK)
            return retval;
    } else {
        old_parent = parent;
    }

    if(strstr(newpath, "/"))
    {
        if((retval = fosLookupParent(parent, &newpath, &new_parent, NULL)) != OK)
            return retval;
    } else {
        new_parent = parent;
    }

    // Now look up the actual file that we are moving...
    if((retval = fosLookup(old_parent, oldpath, &child, NULL) != OK))
        return retval;

    bool different_parent = (old_parent.id != new_parent.id);

    // check to see if we're moving to a subdir...
    if(child.type == TYPE_DIR || child.type == TYPE_DIR_SHARED)
    {
        InodeType p = new_parent;

        if(p.id == child.id) return INVALID;

        while(p.id != 0)
        {
            //FIXME: why is this here? Shouldn't _fosResolve always
            //calculate the server from the path info?
            p.server = fosPathOwner(p, "..");

            retval = _fosResolve(p, "..", 2, &p);
            assert(retval == OK);
            if(p.id == child.id) return INVALID;
        }

        // compute the parent path in case the parent changes
        dot_dot_server = fosPathOwner(child, "..");
    }

    bool newname = (strcmp(oldpath, newpath) != 0);


    // get the dir entry server
    // Q: could this just be obtained from the new parent?
    // A: no, the dir entry goes at the server hash(newp, newpath);
    EVENT_START(HASHING);
    int new_server = fosPathOwner(new_parent, newpath);
    int old_server = fosPathOwner(old_parent, oldpath);
    EVENT_END(HASHING);

    bool different_server = (new_server != old_server);

    // ------------------------------------------------------- //
    // Actions begin below here
    // ------------------------------------------------------- //

    // Add the new mapping...
    if(!different_server)
        retval = fosUpdateMap(new_server, new_parent, newpath, child, true, &exist, old_parent, oldpath);
    else
        retval = fosAddMap(new_server, new_parent, newpath, child, true, &exist);

    bool destroy = (retval == EXIST);
    if(!destroy) assert(retval == OK);

    // remove the old mapping
    if(different_server)
    {
        retval = fosRemoveMap(old_server, old_parent, oldpath);
        assert(retval == OK);
    }

    // adding is done by add map call
    if(DIRECTORY_CACHE)
        DirCache::dc().remove(old_parent, oldpath, strlen(oldpath));


    // if the destination already exists then destroy old inode (note handles
    // orphans for opened files)
    if(destroy)
    {
        retval = fosDestroy(exist.server, exist.id);
        assert(retval == OK);
    }

    // patch up dot dot dir
    if(child.type == TYPE_DIR)
    {
        retval = fosAddMap(dot_dot_server, child, "..", new_parent, true, NULL);
        assert(retval == OK || retval == EXIST);
        retval = OK;
    }

    LT(RENAME, "%llx@%d:%s -> %llx@%d:%s ok.", old_parent.id, old_parent.server, oldpath,
            new_parent.id, new_parent.server, newpath);

    return retval;
}

ssize_t fosDoReadlink(InodeType inode, char *buf, size_t bufsiz)
{
    int retval;
    IdRequest *req;
    DispatchTransaction trans;
    PathReply *reply;
    __attribute__((unused)) size_t size;

    bufsiz = (bufsiz > MAX_FNAME_LEN) ? MAX_FNAME_LEN : bufsiz;

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(inode.server);

    req = (IdRequest *) dispatchAllocate(chan, sizeof(*req));
    req->id = inode.id;
    trans = dispatchRequest(chan, READLINK, req, sizeof(*req));
    hare_wake(inode.server);

    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);

    assert(reply->retval == OK);

    retval = reply->size > bufsiz ? bufsiz : reply->size;

    strncpy(buf, reply->path, bufsiz);

    dispatchFree(trans, reply);

    return retval;
}

ssize_t fosReadlink(InodeType parent, const char *path, char *buf, size_t bufsiz)
{
    OP_COUNTER(READLINK);
    int retval;
    int server = 0;

    InodeType child;
    if((retval = fosLookup(parent, path, &child, NULL)) != OK)
        return retval;

    return fosDoReadlink(child, buf, bufsiz);
}

int fosUTime(InodeType parent, const char* path, const struct utimbuf* times)
{
    OP_COUNTER(UTIMENS);
    int retval;
    InodeType child;

    if((retval = fosLookup(parent, path, &child, NULL)) != OK)
        return retval;

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(child.server);

    struct timespec now;
    if (!times)
        clock_gettime(CLOCK_REALTIME, &now);

    StatusReply *reply;
    UTimeNSRequest *request = static_cast<UTimeNSRequest*>(dispatchAllocate(chan, sizeof(*request)));
    request->inode = child.id;
    if (!times) {
        request->atime = now;
        request->mtime = now;
    } else {
        request->atime.tv_sec = times->actime;
        request->mtime.tv_sec = times->modtime;
    }
    request->flags = 0; // follow symlink to honor POSIX semantic
    DispatchTransaction transaction = dispatchRequest(chan, UTIMENS, request, sizeof(*request));
    hare_wake(child.server);

    size_t size = dispatchReceive(transaction, (void**) &reply);
    EVENT_END(RPC);
    retval = reply->retval;
    dispatchFree(transaction, reply);

    // handle symlinks
    if(retval == ISSYM)
    {
        char link_dest[MAX_FNAME_LEN];
        fosDoReadlink(child, link_dest, MAX_FNAME_LEN);
        return fosUTime(parent, link_dest, times);
    }

    return retval;
}

int fosUTimeNS(InodeType parent, const char* path, const struct timespec times[2], int flags)
{
    OP_COUNTER(UTIMENS);
    int retval;

    if((retval = fosLookupParent(parent, &path, &parent, NULL)) != OK)
        return retval;

    InodeType child;
    if((retval = fosLookup(parent, path, &child, NULL)) != OK)
        return retval;

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(child.server);

    struct timespec now;
    if (!times || times[0].tv_nsec == UTIME_NOW || times[1].tv_nsec == UTIME_NOW)
        clock_gettime(CLOCK_REALTIME, &now);

    StatusReply* reply;
    UTimeNSRequest* request = static_cast<UTimeNSRequest*>(dispatchAllocate(chan, sizeof(*request)));
    request->inode = child.id;
    if (!times) {
        request->atime = now;
        request->mtime = now;
    } else {
        if (times[0].tv_nsec == UTIME_NOW) {
            request->atime = now;
        } else {
            request->atime = times[0];
        }
        if (times[1].tv_nsec == UTIME_NOW) {
            request->mtime = now;
        } else {
            request->mtime = times[1];
        }
    }
    request->flags = flags;

    DispatchTransaction transaction = dispatchRequest(chan, UTIMENS, request, sizeof(*request));
    hare_wake(child.server);

    size_t size = dispatchReceive(transaction, (void**) &reply);
    EVENT_END(RPC);
    retval = reply->retval;
    dispatchFree(transaction, reply);

    // handle symlinks
    if(retval == ISSYM)
    {
        char link_dest[MAX_FNAME_LEN];
        fosDoReadlink(child, link_dest, MAX_FNAME_LEN);
        return fosUTimeNS(parent, link_dest, times, flags);
    }

    return retval;
}

int fosFileSymlink(InodeType parent, const char* path, const char* dest)
{
    OP_COUNTER(SYMLINK);
    int retval;
    int server = 0;

    if (!path || !dest) return -EFAULT;
    if (strlen(path) == 0) return NOT_FOUND;

    if((retval = fosLookupParent(parent, &path, &parent, NULL)) != OK)
        return retval;

    server = fosPathOwner(parent, path);

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(server);

    StatusReply* reply;
    size_t pathlen = strlen(path);
    size_t dest_len = strlen(dest);
    size_t request_size = pathlen + 1 + dest_len + 1 + sizeof(SymlinkRequest);
    SymlinkRequest* request = static_cast<SymlinkRequest*>(dispatchAllocate(chan, request_size));
    request->parent_id = parent.id;
    request->separator = (uint64_t) (pathlen + 1);
    strcpy(&request->paths[0], path);
    strcpy(&request->paths[request->separator], dest);
    DispatchTransaction transaction = dispatchRequest(chan, SYMLINK, request, request_size);
    hare_wake(server);

    size_t size = dispatchReceive(transaction, (void**) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(StatusReply));
    retval = reply->retval;
    dispatchFree(transaction, reply);

    return retval;
}

int fosFileDup(int server, fd_t fd)
{
    OP_COUNTER(DUP);
    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(server);
    DupRequest *req = (DupRequest *)dispatchAllocate(chan, sizeof(*req));
    assert(req);
    req->fd = fd;

    DispatchTransaction trans = dispatchRequest(chan, DUP, req, sizeof(*req));
    hare_wake(server);

    DupReply *reply;
    size_t size = dispatchReceive(trans, (void **)&reply);
    EVENT_END(RPC);

    assert(size == sizeof(*reply));
    int retval = reply->retval;

    Inode inode = reply->inode;
    uint64_t filesize = reply->size;
    int type = reply->type;

    dispatchFree(trans, reply);

    return retval;

}

int _fosResolve(InodeType parent, const char *path, int pathlen, InodeType *inode)
{
    OP_COUNTER(RESOLVE);

    if(parent.id == 0 && (path == NULL || pathlen == 0 || path[0] == '\0'))
    {
        inode->server = 0;
        inode->id = 0;
        inode->type = TYPE_DIR;
        return OK;
    }

    // short circut dot dirs
    if(pathlen == 1 && path[0] == '.')
    {
        *inode = parent;
        inode->type = TYPE_DIR;
        return OK;
    }

    int retval;
    IdPathRequest *req;
    DispatchTransaction trans;
    ResolveReply *reply;
    __attribute__((unused)) size_t size;

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(parent.server);

    req = (IdPathRequest *) dispatchAllocate(chan, sizeof(*req) + pathlen + 1);
    req->id = parent.id;
    strncpy(req->path, path, pathlen);
    req->path[pathlen] = '\0';

    trans = dispatchRequest(chan, RESOLVE, req, sizeof(*req) + pathlen + 1);
    hare_wake(parent.server);

    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);

    retval = reply->retval;

    if(retval == OK) 
    {
        if(inode)  *inode = reply->child;
    }

    dispatchFree(trans, reply);

    return retval;
}

struct resolve_callback_args
{
    bool found;
    uint64_t *offset;
    InodeType child;
    char *buf;
    size_t *bufsiz;
};

static void _resolve_callback(void *data, uint64_t size, void *p)
{
    struct resolve_callback_args *args = (struct resolve_callback_args *)p;

    int consumed = 0;
    int count = 0;
    struct FauxDirent *src_dent = (struct FauxDirent *)data;

    while(consumed < size)
    {
        int namelen = strlen(src_dent->d_name);
        int reclen = ALIGN(offsetof(struct linux_dirent, d_name) + namelen + 2,
                sizeof(long));

        char*path = src_dent->d_name;

        if(src_dent->d_ino == args->child.id)
        {
            args->found = true;
            strncpy(args->buf, path, strlen(path) + 1);
            *(args->bufsiz) = strlen(path);
        }

        // advance the pointers / accumulators
        consumed += src_dent->d_reclen;
        src_dent = (struct FauxDirent *)((char *)src_dent + src_dent->d_reclen);
        count++;
    }
    *(args->offset) += count;
}

int _fosResolvePath(InodeType parent, InodeType child, char *buf, size_t *bufsiz)
{
    int csc = CONFIG_FS_SERVER_COUNT;
    int size_per_server = 49152; /* yes this is a computer number, it is 75% of a typical channel buffer */;  
    uint64_t actual_size;
    bool more[csc];
    uint64_t offsets[csc];
    bool one_has_more = true;
    for(int i = 0; i < csc; i++)
    {
        more[i] = true;
        offsets[i] = 0;
    }

    struct resolve_callback_args args;
    args.found = false;
    args.child = child;
    args.buf = buf;
    args.bufsiz = bufsiz;

    DispatchTransaction dent_trans[csc];
    for(int i = 0; i < csc; i++)
    {
        InodeType dir = { i, parent.id, parent.type };
        dent_trans[i] = fosDirentReq(dir, offsets[i], size_per_server);
    }

    while(one_has_more)
    {
        one_has_more = false;
        for(int i = 0; i < csc; i++)
        {
            if(!more[i]) continue;
            args.offset = &offsets[i];
            int ret = fosDirentWaitReply(dent_trans[i], &actual_size, &more[i], _resolve_callback, &args);
            if(more[i] && !args.found)
            {
                InodeType dir = { i, parent.id, parent.type };
                dent_trans[i] = fosDirentReq(dir, offsets[i], size_per_server);
                one_has_more = true;
            }
        }
    }

    return args.found ? OK : NOT_FOUND;
}

int fosResolveFull(InodeType inode, char *buf, size_t *bufsiz)
{
    assert(buf && bufsiz);

    InodeType child = inode;
    std::vector<std::string> dir_stack;

    int length = 0;
    char curpath[MAX_FNAME_LEN];
    size_t size = sizeof(curpath);
    int retval;

    if(child.id != 0)
    {
        InodeType parent;

        // first resolve the child node...
        child.server = fosPathOwner(child, "..");
        retval = _fosResolve(child, "..", 2, &parent);
        if(retval != OK)
        {
            fprintf(stderr, "well we couldn't really find the parent...?\n");
        }
        if(retval != OK) return retval;

        // now start looking for parents
        while(true)
        {
            retval = _fosResolvePath(parent, child, curpath, &size);
            if(retval != OK) return retval;

//            fprintf(stderr, "cwd got dir: %s\n", buf);

            dir_stack.push_back(curpath);
            length += strlen(curpath) + 1;

            if(parent.id == 0) break;

            child = parent;

            child.server = fosPathOwner(child, "..");
            retval = _fosResolve(child, "..", 2, &parent);
            if(retval != OK) return retval;

        }

    }

    // add the root
    dir_stack.push_back("");
    length++;

    if(*bufsiz < length) return -ERANGE;

    *buf = 0;
    for(int i = dir_stack.size(); i > 0; i--)
    {
        strcat(buf, dir_stack[i - 1].c_str());
        if(i != 1 || dir_stack.size() == 1) // root is just "/"
            strcat(buf, "/");
    }

    *bufsiz = strlen(buf) + 1;
    return OK;
}

int _fosResolvePieces(InodeType parent, const char **path, int pathlen, InodeType *result, bool *cached)
{
    int ret = OK;

    // Resolve the intermediates
    int start_pathlen = pathlen;
    const char *start_path = *path;
    const char *sep;
    *result = parent;
    while(*path[0] && (sep = strchr(*path, '/')) && ret == OK)
    {
        if(sep - start_path >= start_pathlen) break;
        ret = DirCache::dc().resolve(*result, *path, sep - *path, result, cached);
        pathlen -= (sep - *path) + 1;
        *path = sep + 1;
    }

    if(ret != OK) return ret;

    ret = DirCache::dc().resolve(*result, *path, pathlen, result, cached);
    return ret;
}

int fosLookupParent(InodeType parent, const char **path, InodeType *result, bool *cached)
{
    dispatchCheckNotifications();

    OP_COUNTER(LOOKUP);
    int ret = OK;
    if (!*path || !*path[0])
    {
        *result = parent;
        return ret;
    }

    char *lastsep = NULL;

    if (*path[0] == '/')
    {
        parent.id = 0;
        parent.type = TYPE_DIR;
    }

    //FIXME: some way to do this without modifying a char *path?
    // ignore slashes on the end of dirs that we are creating
    int pathlen = strlen(*path);
    if(pathlen > 1)
    {
        lastsep = (char *)&((*path)[pathlen - 1]);
        if(*lastsep == '/') *lastsep = '0';
        else lastsep = NULL;
    }

    const char *sep;
    if ((sep = strrchr(*path, '/')))
    {
        ret = _fosResolvePieces(parent, path, sep - *path, result, cached);
        *path = sep + 1;
    }
    else
        *result = parent;

    if(lastsep) *lastsep = '/';

    if(result->type != TYPE_DIR_SHARED && result->type != TYPE_DIR)
        return -INVALID;

    return ret;
}

int fosLookup(InodeType parent, const char *path, InodeType *result, bool *cached)
{
    dispatchCheckNotifications();

    OP_COUNTER(LOOKUP);
    if(! path) path = "";
    if (path[0] == '/')
    {
        parent.id = 0;
        parent.type = TYPE_DIR;
    }

    int retval = _fosResolvePieces(parent, &path, strlen(path), result, cached);
    assert(result->type != 0);
    return retval;
}

void _fosClearCache(InodeType parent, const char *path, int pathlen)
{
#if DIRECTORY_CACHE
    if (path[0] == '/')
    {
        parent.id = 0;
        parent.type = TYPE_DIR;
        path++;
        pathlen--;
    }

    bool lastsep = false;
    if(path[pathlen - 1] == '/')
    {
        lastsep = true;
        //FIXME: some way to do this without modifying a char *path?
        char *path_edit = (char *)path;
        path_edit[pathlen - 1] = '\0';
        pathlen--;
    }

    int ret = OK;
    // Resolve the intermediates
    int start_pathlen = pathlen;
    const char *start_path = path;
    const char *sep;
    while(path[0] && (sep = strchr(path, '/')) && ret == OK)
    {
        if(sep - start_path >= start_pathlen) break;
        InodeType child;

        ret = DirCache::dc().resolve(parent, path, sep - path, &child, NULL);
        DirCache::dc().remove(parent, path, sep - path);

        parent = child;
        pathlen -= (sep - path) + 1;
        path = sep + 1;
    }

    // Resolve the final parent->child
    DirCache::dc().remove(parent, path, pathlen);
    if(lastsep) 
    {
        //FIXME: some way to do this without modifying a char *path?
        char *path_edit = (char *)path;
        path_edit[strlen(path)] = '/';
    }

#endif
}

int fosFdRead(int server, fd_t fd, char *data, uint64_t *count)
{
    OP_COUNTER(READ);
    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(server);
    assert(chan);
    ReadRequest *req = (ReadRequest *)dispatchAllocate(chan, sizeof(ReadRequest));
    req->fd = fd;
    req->count = *count;

    DispatchTransaction trans = dispatchRequest(chan, READ, req, sizeof(*req));
    hare_wake(server);

    ReadReply *reply;
    size_t size = dispatchReceive(trans, (void **)&reply);
    EVENT_END(RPC);

    assert((int64_t)reply->count >= 0);
    assert(size == sizeof(ReadReply) + reply->count);

    *count =  reply->count;
    int ret = reply->retval;
    if(*count) memcpy(data, reply->data, *count);

    dispatchFree(trans, reply);

    return ret;
}

int fosFdWrite(int server, fd_t fd, char *data, uint64_t *count)
{
    OP_COUNTER(WRITE);
    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(server);
    WriteRequest *req = (WriteRequest *)dispatchAllocate(chan, sizeof(*req) + *count);
    assert(req);
    req->fd = fd;
    req->count = *count;
    memcpy(req->data, data, *count);

    DispatchTransaction trans = dispatchRequest(chan, WRITE, req, sizeof(*req) + *count);
    hare_wake(server);

    WriteReply *reply;
    size_t size = dispatchReceive(trans, (void **)&reply);
    EVENT_END(RPC);

    assert(size == sizeof(WriteReply));

    *count = reply->count;
    int ret = reply->retval;
    dispatchFree(trans, reply);

    return ret;
}

int fosFdClose(int server, fd_t fd, off64_t offset, uint64_t filesize)
{
    OP_COUNTER(CLOSE);
    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(server);
    CloseRequest *req = (CloseRequest *)dispatchAllocate(chan, sizeof(*req));
    assert(req);
    req->fd = fd;
    req->offset = offset;
    req->size = filesize;
    LT(CLOSE, "%llx", fd);

    DispatchTransaction trans = dispatchRequest(chan, CLOSE, req, sizeof(*req));
    hare_wake(server);

    StatusReply *reply;
    size_t size = dispatchReceive(trans, (void **)&reply);
    EVENT_END(RPC);

    assert(size == sizeof(*reply));
    int retval = reply->retval;
    dispatchFree(trans, reply);

    assert(retval == OK);
    return OK;
}

off64_t fosFdLseek(int server, fd_t fd, off64_t offset, int whence)
{
    OP_COUNTER(LSEEK);
    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(server);
    LseekRequest *req = (LseekRequest *)dispatchAllocate(chan, sizeof(*req));
    req->fd = fd;
    req->offset = offset;
    req->whence = whence;
    DispatchTransaction trans = dispatchRequest(chan, LSEEK, req, sizeof(*req));
    hare_wake(server);

    LseekReply *reply;
    size_t size = dispatchReceive(trans, (void **)&reply);
    EVENT_END(RPC);
    assert(size == sizeof(LseekReply));
    assert(reply->retval == OK);

    off64_t ret = reply->offset;
    dispatchFree(trans, reply);
    return ret;
}

DispatchTransaction fosDirentReq(InodeType inode, off64_t offset, uint64_t size)
{
    DirEntRequest *req;

    // Cap the size of dirent requests since channels aren't as big as the size
    // that apps are typically requesting
    if(size > FS_CHUNK_SIZE) size = FS_CHUNK_SIZE;

    Channel *chan = establish_fs_chan(inode.server);
    req = (DirEntRequest *) dispatchAllocate(chan, sizeof(*req));
    req->inode = inode.id;
    req->offset = offset;
    req->size = size;
    DispatchTransaction ret = dispatchRequest(chan, DIRENT, req, sizeof(*req));

    hare_wake(inode.server);
    return ret;
}


int fosDirentWaitReply(DispatchTransaction trans, uint64_t *size, bool *more, void (*_callback)(void *, uint64_t, void *), void *p)
{
    OP_COUNTER(DIRENT);
    DirEntReply *reply;
    __attribute__((unused)) size_t reply_size;

    reply_size = dispatchReceive(trans, (void **) &reply);
    assert(reply_size >= sizeof(*reply));
    assert(reply_size = sizeof(*reply) + reply->size);

    *size = reply->size;
    int retval = reply->retval;

    if(reply->retval == -1 || reply->retval == -2)
    {
        *size = 0;
        *more = false;
        goto nocopy;
    }

    *more = reply->more;

    _callback(reply->buf, *size, p);

nocopy:
    dispatchFree(trans, reply);
    return retval;
}

struct dent_callback_arg
{
    InodeType parent;
    void *dest;
};

void fosDirentCallback(void *buf, uint64_t size, void *p)
{
    int consumed = 0;

    struct dent_callback_arg *args = (struct dent_callback_arg *)p;
    struct FauxDirent *src_dent = (struct FauxDirent *)buf;

    void *dest = args->dest;

    while(consumed < size)
    {
        int namelen = strlen(src_dent->d_name);
        int reclen = ALIGN(offsetof(struct linux_dirent, d_name) + namelen + 2,
                sizeof(long));
        char*path = src_dent->d_name;

        if((src_dent->d_type == DT_DIR || src_dent->d_type == DT_BLK) && path[0] != '.')
        {
// Can only add this entry if we're using directory distribution
// otherwise, the calculation for the child's server is incorrect
// instead it needs to be provided in the dent (which requires changing
// the dent structure)
// or perhaps steal it from the inode?
            InodeType child = { (int)(src_dent->d_ino >> INODE_SHIFT), src_dent->d_ino, src_dent->d_type == DT_DIR ? TYPE_DIR : TYPE_DIR_SHARED };
            DirCache::dc().add(args->parent, path, strlen(path), child);
        }

        if(src_dent->d_type == DT_BLK) src_dent->d_type = DT_DIR;

        consumed += src_dent->d_reclen;
        src_dent = (struct FauxDirent *)((char *)src_dent + src_dent->d_reclen);
    }
    memcpy(dest, buf, size);
}

int fosDirent(InodeType inode, off64_t offset, uint64_t *size, void *buf, bool *more)
{
    DispatchTransaction trans = fosDirentReq(inode, offset, *size);
    struct dent_callback_arg args;
    args.dest = buf;
    args.parent = inode;
    return fosDirentWaitReply(trans, size, more, fosDirentCallback, &args);
}

int fosGetBlocklist(Inode inode, uint64_t offset, uint64_t count, uintptr_t *list)
{
    OP_COUNTER(BLOCKS);
    int retval;
    BlockListRequest *req;
    DispatchTransaction trans;
    BlockListReply *reply;
    __attribute__((unused)) size_t size;

    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(inode.server);

    req = (BlockListRequest *) dispatchAllocate(chan, sizeof(*req));
    req->offset = offset;
    req->count = count;
    req->inode = inode.id;

    trans = dispatchRequest(chan, BLOCKS, req, sizeof(*req));
    hare_wake(inode.server);

    size = dispatchReceive(trans, (void **) &reply);
    EVENT_END(RPC);
    assert(size == sizeof(*reply));

    retval = reply->retval;
    assert(retval == OK);

    memcpy(list, reply->blocks, count * sizeof(uintptr_t));

    dispatchFree(trans, reply);

    return retval;
}

int fosFdFlush(int server, fd_t fd, uint64_t filesize, off64_t offset)
{
    OP_COUNTER(FLUSH);
    LT(ALL, "%llx %lld", fd, offset);
    EVENT_START(RPC);
    Channel *chan = establish_fs_chan(server);

    FlushRequest *req = (FlushRequest *)dispatchAllocate(chan, sizeof(*req));
    assert(req);
    req->fd = fd;
    req->offset = offset;
    req->size = filesize;

    DispatchTransaction trans = dispatchRequest(chan, FLUSH, req, sizeof(*req));
    hare_wake(server);

    StatusReply *reply;
    size_t size = dispatchReceive(trans, (void **)&reply);
    EVENT_END(RPC);

    assert(size == sizeof(*reply));
    int retval = reply->retval;
    dispatchFree(trans, reply);

    assert(retval == OK);
    return retval;
}

extern char *g_progname;
void fosCleanup()
{
    static bool exited = false;
    if(exited) return;
    exited = true;

#if ENABLE_DBG_CLIENT_OP_COUNTER
    char *simple_name = strrchr(g_progname, '/');
    if(simple_name) g_progname = simple_name + 1;

    if(strcmp(g_progname, "directories") == 0)
        fosShowLocalOps();
#endif
}

void fosNotifications(void *data, size_t size)
{
    assert(size == sizeof(Inode));
    Inode *inode = (Inode *)data;
    DirCache::dc().remove_dest(*inode);
}
