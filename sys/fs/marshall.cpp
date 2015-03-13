#include <cassert>
#include <cstdio>

#include <messaging/dispatch.h>

#include "marshall.hpp"
#include "server.hpp"

namespace fos { namespace fs {


static inline uint64_t __rdtscll()
{
    uint32_t low_, high_;

    __asm__ __volatile__ ("rdtsc" : "=a" (low_), "=d" (high_));
    return (((uint64_t) high_ << 32) | (uint64_t) low_);
}

#if ENABLE_DBG_SHOW_QUEUE_DELAY
extern "C" {
extern uint64_t g_msg_count;
extern uint64_t g_queue_time;
extern uint64_t g_last_msg_time;
}
static inline void calc_queue_delay(void *vpmsg)
{
    _DispatchHeader *hdr = (_DispatchHeader *)(((char *)vpmsg) - sizeof(*hdr));
    uint64_t now = __rdtscll();

    uint64_t send_time = hdr->header.send_time;

    if(g_last_msg_time && send_time < g_last_msg_time)
        send_time = g_last_msg_time;

    uint64_t queue_time = now - send_time;
    g_queue_time += queue_time;
    g_msg_count++;
    g_last_msg_time = now;
}
#define CALC_QUEUE_DELAY(vpmsg) do { calc_queue_delay(vpmsg); } while (0)
#else
#define CALC_QUEUE_DELAY(vpmsg) do { } while (0)
#endif

#if ENABLE_DBG_CHECK_QUEUES
extern "C" {
extern uint64_t g_times_checked;
extern uint64_t g_times_not_empty;
extern uint64_t g_times_total;
}
static inline void check_queues()
{
    g_times_checked++;
    int count = dispatchCheckQueues();
    if(count)
    {
        g_times_not_empty++;
        g_times_total += count;
    }
}
#define CHECK_QUEUES() do { check_queues(); } while (0)
#else
#define CHECK_QUEUES() do { } while (0)
#endif

void closeCallback(Channel* chan)
{
    FilesystemServer & fs = FilesystemServer::fs();
    fs.appClose((uintptr_t)chan);
}

void pipe(Channel* channel, DispatchTransaction trans, void* vpmsg, size_t size_)
{
    CHECK_QUEUES();
    assert(vpmsg);

    //nothing interesting in the requst
    //PipeRequest* request = static_cast<PipeRequest*>(vpmsg);
    PipeReply* reply = static_cast<PipeReply*>(dispatchAllocate(channel, sizeof(*reply)));

    fd_t read_fd;
    fd_t write_fd;
    Inode inode;
    reply->retval = FilesystemServer::fs().pipe(read_fd, write_fd, inode);
    if(reply->retval != OK) RT(ALL, "rc: %d", reply->retval);

    reply->read_fd = read_fd;
    reply->write_fd = write_fd;
    reply->inode = inode;

    dispatchFree(trans, vpmsg);
    dispatchRespond(channel, trans, reply, sizeof(*reply));
}

void create(Channel* channel, DispatchTransaction trans, void* vpmsg, size_t size_)
{
    CHECK_QUEUES();
    assert(vpmsg);
    FilesystemServer & fs = FilesystemServer::fs();

    CreateRequest* request = static_cast<CreateRequest*>(vpmsg);
    StatusInodeTypeReply* reply = static_cast<StatusInodeTypeReply*>(dispatchAllocate(channel, sizeof(*reply)));

    int flags = request->flags;

    InodeType exist;
    InodeType parent = request->parent;

    char path[MAX_FNAME_LEN];
    int retval = fs.create(InodeOnly(parent), request->path, flags, request->mode, exist);
    reply->retval = retval;
    reply->inode = exist;

    // we need the path to add the map so copy it before we free the xaction
    if(retval == OK && flags & O_DIRECTORY) strcpy(path, request->path);

    dispatchFree(trans, vpmsg);
    dispatchRespond(channel, trans, reply, sizeof(*reply));

    // since directory inodes are are always created at their
    // directory entry servers
    if(retval == OK && flags & O_DIRECTORY)
    {
        unsigned int dot_dot_server;

        if(exist.type == TYPE_DIR_SHARED)
            dot_dot_server = fosPathOwner(exist, "..");
        else
            dot_dot_server = exist.server;

        // we just added a new directory, so add the mapping as well
        if(parent.server == exist.server || parent.type == TYPE_DIR_SHARED)
        {
            retval = fs.addmap((uintptr_t)channel, InodeOnly(parent), path, exist, false, &exist);
            assert(retval == OK);
        }

        // add the mapping for .. if it lives local
        if(dot_dot_server == fs.id)
        {
            retval = fs.addmap((uintptr_t)channel, InodeOnly(exist), "..", parent, false, &exist);
            assert(retval == OK);
        }

    }
}

void destroy(Channel* channel, DispatchTransaction trans, void* vpmsg, size_t size_)
{
    CHECK_QUEUES();
    assert(vpmsg);

    IdRequest* request = static_cast<IdRequest*>(vpmsg);
    StatusReply* reply = static_cast<StatusReply*>(dispatchAllocate(channel, sizeof(*reply)));

    reply->retval = FilesystemServer::fs().destroy(request->id);
    if(reply->retval != OK) RT(ALL, "rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(channel, trans, reply, sizeof(*reply));
}

void addmap(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);

    AddMapRequest * msg = (AddMapRequest *)(vpmsg);
    StatusInodeTypeReply * reply = (StatusInodeTypeReply *) dispatchAllocate(chan, sizeof(*reply));

    // if we're updatunig we add then remove, incoming message changes structure
    if(msg->update)
    {
        UpdateMapRequest * msg2 = (UpdateMapRequest *)(vpmsg);
        reply->retval = FilesystemServer::fs().addmap((uintptr_t)chan, InodeOnly(msg2->parent), msg2->path, msg2->child, msg2->overwrite, &reply->inode);
        if(reply->retval != OK) RT(ALL, "rc: %d", reply->retval);

        assert(reply->retval == OK || reply->retval == EXIST);

        int remove_retval;
        char *old_path = msg2->path + strlen(msg2->path) + 1;
        remove_retval = FilesystemServer::fs().removemap((uintptr_t)chan, InodeOnly(msg2->old_parent).id, old_path);
        if(remove_retval != OK)
            T("update map error removing: %s after adding: %s\n", old_path, msg2->path);

        assert(remove_retval == OK);
    } else {
        reply->retval = FilesystemServer::fs().addmap((uintptr_t)chan, InodeOnly(msg->parent), msg->path, msg->child, msg->overwrite, &reply->inode);
        if(reply->retval != OK) RT(ALL, "rc: %d", reply->retval);

        assert(reply->retval == OK || reply->retval == EXIST);
    }

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void removemap(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);

    IdPathRequest * msg = (IdPathRequest *)(vpmsg);
    StatusReply * reply = (StatusReply *) dispatchAllocate(chan, sizeof(*reply));

    reply->retval = FilesystemServer::fs().removemap((uintptr_t)chan, msg->id, msg->path);
    if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void open(Channel* channel, DispatchTransaction trans, void* vpmsg, size_t size_)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    OpenReply* reply = static_cast<OpenReply*>(dispatchAllocate(channel, sizeof(*reply)));
    fd_t fd;
    size_t size;
    int state;
    int retval;
    int type;

    InodeType exist;
    FilesystemServer & fs = FilesystemServer::fs();

    if(((OpenRequest *)vpmsg)->flags & O_CREAT)
    {
        auto request = static_cast<CreateOpenRequest*>(vpmsg);

        // create the inode
        retval = fs.create(request->parent, request->path, request->flags, request->mode, exist);

        bool excl = (request->flags & O_EXCL);

        if((retval == EXIST && ! excl) || retval == OK)
        {
            // add the map
            if(retval != EXIST)
                retval = fs.addmap((uintptr_t)channel, InodeOnly(request->parent), request->path, exist, false, &exist);
            else
            {
                fs.appAddCache((uintptr_t)channel, request->parent, request->path);
                retval = OK;
            }

            // if we just created it ok, then adding the map should always work
            assert(retval == OK);

            // NOTE: if the file exists on this server then we can open it immediately. Otherwise
            // we must call open on the exist server.
            if(exist.server == (int)FilesystemServer::id)
                retval = fs.open(exist.id, request->flags & ~O_CREAT, fd, type, size, state);
            else
                retval = EXIST;
        }
    }
    else
    {
        OpenRequest* request = static_cast<OpenRequest*>(vpmsg);
        retval = fs.open(request->id, request->flags, fd, type, size, state);
    }

    reply->retval = retval;
    reply->fd = fd;
    reply->type = type;
    reply->size = size;
    reply->state = state;
    reply->exist = exist;
    //if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(channel, trans, reply, sizeof(*reply));
}


void read(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    ReadRequest * msg = (ReadRequest *)(vpmsg);
    ReadReply * reply = (ReadReply *) dispatchAllocate(chan, sizeof(*reply) + msg->count);
    reply->count = msg->count;

    fd_t fd = msg->fd;

    reply->retval = FilesystemServer::fs().read(fd, reply->data, &reply->count);
    if(reply->retval != OK && reply->retval != -EAGAIN) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply) + reply->count);
}

void write(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    WriteRequest * msg = (WriteRequest *)(vpmsg);
    WriteReply * reply = (WriteReply *) dispatchAllocate(chan, sizeof(*reply));
    reply->count = msg->count;

    reply->retval = FilesystemServer::fs().write(msg->fd, msg->data, &reply->count);
    if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void lseek(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    LseekRequest * msg = (LseekRequest *)(vpmsg);
    LseekReply * reply = (LseekReply *) dispatchAllocate(chan, sizeof(*reply));

    uint64_t offset = msg->offset;
    reply->retval = FilesystemServer::fs().lseek(msg->fd, offset, msg->whence);
    reply->offset = offset;
    if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void dup(Channel* channel, DispatchTransaction trans, void* vpmsg, size_t size_)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    DupRequest* request = static_cast<DupRequest*>(vpmsg);
    DupReply* reply = static_cast<DupReply*>(dispatchAllocate(channel, sizeof(*reply)));

    Inode inode;
    int type;
    uint64_t size;
    reply->retval = FilesystemServer::fs().dup(request->fd, inode, type, size);
    if(reply->retval != OK) T("rc: %d", reply->retval);
    reply->inode = inode;
    reply->type = type;
    reply->size = size;
    dispatchFree(trans, vpmsg);
    dispatchRespond(channel, trans, reply, sizeof(*reply));
}

void close(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    CloseRequest * msg = (CloseRequest *)(vpmsg);
    StatusReply * reply = (StatusReply *) dispatchAllocate(chan, sizeof(*reply));

    reply->retval = FilesystemServer::fs().close(msg->fd, msg->offset, msg->size);
    if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void fstat(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    IdRequest * msg = (IdRequest *)(vpmsg);
    StatReply * reply = (StatReply *) dispatchAllocate(chan, sizeof(*reply));

    reply->retval = FilesystemServer::fs().fstatat(msg->id, &reply->buf);
    //if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void direntry(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    DirEntRequest * msg = (DirEntRequest *)(vpmsg);
    DirEntReply * reply = (DirEntReply *) dispatchAllocate(chan, sizeof(*reply) + msg->size);
    reply->size = msg->size;

    reply->retval = FilesystemServer::fs().direntry((uintptr_t)chan, msg->inode, msg->offset, &reply->size, (char *)reply->buf, &reply->more);
    //if(reply->retval != OK && reply->retval < 0) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void unlink(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    UnlinkRequest * msg = (UnlinkRequest *)(vpmsg);
    StatusReply * reply = (StatusReply *) dispatchAllocate(chan, sizeof(*reply));

    reply->retval = OK;

    // if a parent was provided then we want to remove the mapping
    // as well as unlinking the file.
    if(msg->parent_id != UINT64_MAX)
        reply->retval = FilesystemServer::fs().removemap((uintptr_t)chan, msg->parent_id, msg->path);

    if(reply->retval == OK)
        reply->retval = FilesystemServer::fs().unlink(msg->id);

    if(reply->retval != OK) T("[%d] rc: %d (%llx)", FilesystemServer::fs().id, reply->retval, msg->id);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void rmdir(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    RmdirRequest * msg = (RmdirRequest *)(vpmsg);
    StatusReply * reply = (StatusReply *) dispatchAllocate(chan, sizeof(*reply));

    reply->retval = FilesystemServer::fs().rmdir(msg->id, msg->phase == RMDIR_AND_MAP ? RMDIR_FORCE : msg->phase);
    //if(reply->retval != OK) T("rc: %d", reply->retval);
    if(reply->retval == OK && msg->phase == RMDIR_AND_MAP)
    {
        RmdirAndMapRequest * msg2 = (RmdirAndMapRequest *)(vpmsg);
        reply->retval = FilesystemServer::fs().removemap((uintptr_t)chan, InodeOnly(msg2->parent).id, msg2->path);
    }

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void readlink(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    IdRequest * msg = (IdRequest *)(vpmsg);
    PathReply * reply = (PathReply *) dispatchAllocate(chan, sizeof(*reply) + MAX_FNAME_LEN);

    reply->retval = FilesystemServer::fs().readlink(msg->id, reply->path, &reply->size);
    if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void truncate(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    TruncRequest * msg = (TruncRequest *)(vpmsg);
    StatusReply * reply = (StatusReply *) dispatchAllocate(chan, sizeof(*reply));

    reply->retval = FilesystemServer::fs().truncate(msg->inode, msg->len);
    if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void chmod(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    ChmodRequest * msg = (ChmodRequest *)(vpmsg);
    StatusReply * reply = (StatusReply *) dispatchAllocate(chan, sizeof(*reply));

    reply->retval = FilesystemServer::fs().chmod(msg->inode, msg->mode);
    if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void chown(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    ChownRequest * msg = (ChownRequest *)(vpmsg);
    StatusReply * reply = (StatusReply *) dispatchAllocate(chan, sizeof(*reply));

    reply->retval = FilesystemServer::fs().chown(msg->inode, msg->uid, msg->gid);
    if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void resolve(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    auto msg = (IdPathRequest *)(vpmsg);
    ResolveReply * reply = (ResolveReply *) dispatchAllocate(chan, sizeof(*reply));

    reply->retval = FilesystemServer::fs().resolve((uintptr_t)chan, msg->id, msg->path, &reply->child);
    //if(reply->retval != OK) T("rc: %d path: %s", reply->retval, msg->path);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply) + reply->size);
}

void utimens(Channel* channel, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    UTimeNSRequest* request = (UTimeNSRequest*) vpmsg;
    StatusReply* reply = (StatusReply*) dispatchAllocate(channel, sizeof(*reply));
    reply->retval = FilesystemServer::fs().utimens(request->inode, &request->atime, &request->mtime, request->flags);
    if(reply->retval != OK) T("rc: %d", reply->retval);
    dispatchFree(trans, vpmsg);
    dispatchRespond(channel, trans, reply, sizeof(*reply));
}

void symlink(Channel* channel, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    SymlinkRequest* request = (SymlinkRequest*) vpmsg;
    StatusReply* reply = (StatusReply*) dispatchAllocate(channel, sizeof(*reply));
    reply->retval = FilesystemServer::fs().symlink(request->parent_id, &request->paths[0], &request->paths[request->separator]);
    if(reply->retval != OK) T("rc: %d", reply->retval);
    dispatchFree(trans, vpmsg);
    dispatchRespond(channel, trans, reply, sizeof(*reply));
}

void blocks(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    BlockListRequest* req = static_cast<BlockListRequest*>(vpmsg);
    BlockListReply* reply = static_cast<BlockListReply*>(dispatchAllocate(chan, sizeof(*reply) + req->count * sizeof(uintptr_t)));
    reply->count = req->count;
    reply->retval = FilesystemServer::fs().blocks(req->inode, reply->blocks, req->offset, req->count);
    if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

void flush(Channel* chan, DispatchTransaction trans, void* vpmsg, size_t size)
{
    CHECK_QUEUES();
    assert(vpmsg);
    CALC_QUEUE_DELAY(vpmsg);

    FlushRequest* req = static_cast<FlushRequest*>(vpmsg);
    StatusReply* reply = static_cast<StatusReply*>(dispatchAllocate(chan, sizeof(*reply)));
    reply->retval = FilesystemServer::fs().flush(req->fd, req->size, req->offset);
    if(reply->retval != OK) T("rc: %d", reply->retval);

    dispatchFree(trans, vpmsg);
    dispatchRespond(chan, trans, reply, sizeof(*reply));
}

}}
