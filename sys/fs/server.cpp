#include <stddef.h>
#include <fcntl.h>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <ctime>
#include <utime.h>
#include <vector>

#include <config/config.h>
#include <messaging/dispatch.h>
#include <messaging/channel.h>
#include <messaging/channel_interface.h>
#include <sys/fs/fs.h>

#include "server.hpp"
#include "marshall.hpp"

#include <utilities/tsc.h>
#include <utilities/debug.h>

#define ROOT_INO 0

#define ALIGN(x, a)            ALIGN_MASK(x, (typeof(x))(a) - 1)
#define ALIGN_MASK(x, mask)    (((x) + (mask)) & ~(mask))

#define RTOUT(errlevel,rc,...) ({ RT(errlevel,__VA_ARGS__); rc; })

using namespace std;
using namespace fos;
using namespace fs;

#if DBG_SHOW_OPS
#define OP_COUNTER(op_name) OpCounter _c(op_name);
#else
#define OP_COUNTER(op_name)
#endif

#if ENABLE_DBG_CHECK_QUEUES
extern "C" {
uint64_t g_times_checked = 0;
uint64_t g_times_not_empty = 0;
uint64_t g_times_total = 0;
}
#endif

#if DBG_SHOW_QUEUE_DELAY
extern "C" {
uint64_t g_msg_count = 0;
uint64_t g_queue_time = 0;
uint64_t g_last_msg_time = 0;
}
#endif

class comma_numpunct : public std::numpunct<char>
{
protected:
    virtual char do_thousands_sep() const
    {
        return ',';
    }

    virtual std::string do_grouping() const
    {
        return "\03";
    }
};

FilesystemServer *FilesystemServer::g_fs = NULL;
unsigned int FilesystemServer::id;

void fs_idle()
{
    FilesystemServer & fs = FilesystemServer::fs();
    fs.idle();
}

FilesystemServer::~FilesystemServer()
{
}

FilesystemServer::FilesystemServer(Endpoint *ep, int _id)
: m_id(_id)
, m_master(_id == 0)
, m_root("", S_IRWXU | S_IRGRP| S_IROTH | S_IXGRP | S_IXOTH)
, m_orphans("orphans", S_IRWXU | S_IRGRP| S_IROTH | S_IXGRP | S_IXOTH)
{
    assert(g_fs == NULL);
    g_fs = this;

    if(!m_master)
        usleep(50000);

    m_buffer_cache = make_shared<BufferCache>(m_id);

    if(m_id == 0)
    {
        m_root.id(0);
        //m_root.server(m_id);
        InodeType root = {  m_id, 0, TYPE_DIR };
        m_root.addChild("..", root);
        m_files[0] = &m_root;
    }

    m_orphans.id(-1);
    //m_orphans.server(m_id);

    m_files[-1] = &m_orphans;

    dispatchRegister(CREATE, fs::create);
    dispatchRegister(DESTROY, fs::destroy);
    dispatchRegister(ADDMAP, fs::addmap);
    dispatchRegister(REMOVEMAP, fs::removemap);
    dispatchRegister(PIPE, fs::pipe);
    dispatchRegister(OPEN, fs::open);
    dispatchRegister(READ, fs::read);
    dispatchRegister(WRITE, fs::write);
    dispatchRegister(LSEEK, fs::lseek);
    dispatchRegister(CLOSE, fs::close);
    dispatchRegister(DUP, fs::dup);
    dispatchRegister(FSTAT, fs::fstat);
    dispatchRegister(UNLINK, fs::unlink);
    dispatchRegister(DIRENT, fs::direntry);
    dispatchRegister(READLINK, fs::readlink);
    dispatchRegister(RMDIR, fs::rmdir);
    dispatchRegister(TRUNC, fs::truncate);
    dispatchRegister(CHMOD, fs::chmod);
    dispatchRegister(CHOWN, fs::chown);
    dispatchRegister(RESOLVE, fs::resolve);
    dispatchRegister(UTIMENS, fs::utimens);
    dispatchRegister(SYMLINK, fs::symlink);
    dispatchRegister(BLOCKS, fs::blocks);
    dispatchRegister(FLUSH, fs::flush);

#if DIRECTORY_CACHE
    dispatchSetCloseCallback(fs::closeCallback);
#endif
}

void FilesystemServer::appClose(uintptr_t appId)
{
    m_apps.erase(appId);
}

void FilesystemServer::appAddCache(uintptr_t appId, VFileLoc *entry)
{
    m_apps[appId].insert(entry);
}

void FilesystemServer::appAddCache(uintptr_t appId, Inode parent, const char *path)
{
    auto p_i = m_files.find(parent.id);
    assert(p_i != m_files.end());
    assert(p_i->second->isDir());
    Dir *p = static_cast<Dir *>(p_i->second);
    auto c_i = p->find(path);
    assert(c_i != p->end());
    VFileLoc *vfl_p = c_i->second;
    appAddCache(appId, vfl_p);
}

void FilesystemServer::appRemoveCache(uintptr_t appId, VFileLoc *entry)
{
    for(auto i = m_apps.begin(); i != m_apps.end(); i++)
    {
        auto e = i->second.find(entry);
        if(e != i->second.end())
        {
            i->second.erase(e);
            if(i->first != appId)
            {
                Channel *channel = (Channel*)(i->first);
                InodeNotify* notify = static_cast<InodeNotify*>(dispatchAllocate(channel, sizeof(*notify)));
                notify->inode = InodeOnly(entry->inode);
                dispatchNotify(channel, notify, sizeof(*notify));
            }
        }
    }
}

int FilesystemServer::create(Inode parent, const char *path, int flags, mode_t mode, InodeType &exist)
{
    RT(CREATE, "[%d] Create: %llx@%d:%s", m_id, parent.id, parent.server, path);
    OP_COUNTER(CREATE);

    Dir* parent_p;

    auto p_i = m_files.find(parent.id);

    // lazy allocate a parent directory
    if(p_i == m_files.end())
    {
        Dir *lazyp = new Dir(true, 0777);
        lazyp->id(parent.id);
        m_files[parent.id] = lazyp;

        p_i = m_files.find(parent.id);
    }

    if(! p_i->second->isDir()) return RTOUT(CREATE,INVALID,"[%d] %llx:%s -> INVALID", m_id, parent.id, path);
    if(p_i == m_files.end()) return RTOUT(CREATE,NOT_FOUND,"[%d] %llx:%s -> NOT_FOUND", m_id, parent.id, path);

    parent_p = (Dir *)p_i->second;

    assert(strlen(path) > 0);
    auto vf_i = parent_p->find(path);

    if (vf_i != parent_p->end()) { // real_path exists
        VFileLoc *vfileloc = vf_i->second;
        exist.id = vfileloc->inode.id;
        exist.server = vfileloc->inode.server;
        exist.type = vfileloc->inode.type;


        if (vfileloc->isDir() && ! flags & O_DIRECTORY) return RTOUT(CREATE,ISDIR,"isdir");
        if (vfileloc->isSymlink()) return ISSYM;

        return RTOUT(CREATE,EXIST,"[%d] %llx:%s -> EXIST (%llx@%d)", m_id, parent.id, path, exist.id, exist.server);
    } else { // doesn't exist
        VFile *vfile;
        if(flags & O_DIRECTORY)
        {
            vfile = new Dir(false, mode);
            assert(vfile);
        }
        else
            vfile = new File(m_buffer_cache, mode);

        assert(vfile);

        exist.id = vfile->id();
        exist.server = m_id;
        exist.type = vfile->type();


        // install file
        m_files[exist.id] = vfile;
    }

    RT(CREATE, "[%d] %llx:%s -> %llx (type: %d)", m_id, parent.id, path, exist.id, exist.type);
    return OK;
}

int FilesystemServer::destroy(file_id_t inode)
{
    RT(DESTROY, "%llx", inode);
    OP_COUNTER(DESTROY);
    auto vf_i = m_files.find(inode);
    if(vf_i == m_files.end()) return NOT_FOUND;
    VFile *c = vf_i->second;

    if(c->isDir())
    {
        m_files.erase(vf_i);               // erase from server
        delete c;
        return OK;
    }

    bool anonymous = false;
    for(auto fd_i = m_fds.begin(); fd_i != m_fds.end(); fd_i++)
    {
        if(fd_i->second->id() == c->id() && fd_i->second->count())
        {
            //T("[%s] refs: %d", vf_p->name().c_str(), fd_i->second->count());
            anonymous = true;
            break;
        }
    }

    if(anonymous)
    {
        char newname[MAX_FNAME_LEN];
        sprintf(newname, "f.%lld", rdtscll());
        InodeType child = { m_id, c->id(), c->type() };
        m_orphans.addChild(newname, child);
    } else {
        m_files.erase(vf_i);               // erase from server
        delete c;
    }
    return OK;
}

int FilesystemServer::open(file_id_t inode, int flags, fd_t& fd, int& type, size_t& size, int& state)
{
    RT(OPEN, "[%d] %llx", m_id, inode);
    int ret = OK;
    state = CACHE_INVALID;
    VFile* vfile = NULL;
    FileDescriptor *newfd = NULL;
    ChildMap::iterator vf_i;

    OP_COUNTER(OPEN);

    auto vf_p = m_files.find(inode);
    if(vf_p == m_files.end()) { ret = NOT_FOUND; goto done; }

    vfile = vf_p->second;
    if (vfile->isFile()) {
        if(flags & O_DIRECTORY) { ret = NOT_DIR; goto done; }
    }
    else if (vfile->isDir()) {
        // can't use file flags to open a directory.
        if ((flags & (O_CREAT | O_TRUNC | O_WRONLY | O_RDWR))) { ret = ISDIR; goto done; }
    }
    else if (vfile->isSymlink()) {
        //FIXME: just retrun symlink info, make the client
        //do a readlink() and then call open on that path
        //since it has to be resolved anyway...
        type = vfile->type();
        ret = OK; goto done;
    }

    // add fd to fd map and  prepare return values;
    type = vfile->type();
    newfd = new FileDescriptor(inode);
    m_fds[newfd->fd()] = newfd;
    fd = newfd->fd();

    if (vfile->isFile())
    {
        File* file = static_cast<File*>(vfile);
        if(flags & O_TRUNC)
        {
            struct timespec now;
            clock_gettime(CLOCK_REALTIME, &now);
            file->ctime(now);
            file->atime(now);
            file->mtime(now);
            file->truncate(0);
        }
        if(flags & O_APPEND)
            newfd->offset(vfile->size());
    }

    size = vfile->size();

    //FIXME: server doesn't actually know the state now
    //state = (flags & O_WRONLY || flags & O_RDWR) ? CACHE_EXCLUSIVE : CACHE_SHARED;
    // Start out in a shared state. On the library end, if the file
    // is ever modified it moves to an "CACHE_EXCLUSIVE" state that it
    // will have to flush.
    //
    // If the fd is ever shared (dup'd) then it will move to an INVALID state
    // (since it cannot be cached) and then be flushed.
    // see lib/file.cpp
    state = CACHE_SHARED;

#if DBG_SHOW_FDS
    m_fds[fd]->name();
#endif

done:
    RT(OPEN, "[%d] %llx -> fd:%llx type: %d ret: %d", m_id, inode, fd, type, ret);
    return ret;
}

int FilesystemServer::pipe(fd_t& read_fd, fd_t& write_fd, Inode &inode)
{
    OP_COUNTER(PIPE);
    VFile *vfile = new Pipe(0777);

    FileDescriptor *newfd_read = new FileDescriptor(vfile->id());
    read_fd = newfd_read->fd();

    FileDescriptor *newfd_write = new FileDescriptor(vfile->id());
    write_fd = newfd_write->fd();

    inode.id = vfile->id();
    inode.server = m_id;

    Pipe & p = static_cast<Pipe &>(*vfile);
    p.readFd(newfd_read->fd());
    p.writeFd(newfd_write->fd());

    // install the new objects
    m_fds[newfd_read->fd()] = newfd_read;
    m_fds[newfd_write->fd()] = newfd_write;
    m_pipes[vfile->id()] = vfile;

    RT(PIPE, "[%d] read_fd:%llx write_fd: %llx ret: %d", m_id, read_fd, write_fd, OK);
    return OK;
}

int FilesystemServer::read(file_id_t inode, uint64_t offset, char *buf, uint64_t *count)
{
    OP_COUNTER(READ);
    auto vf_p = m_files.find(inode);
    ssize_t retval;
    if(vf_p == m_files.end())
    {
        vf_p = m_pipes.find(inode);
        if(vf_p == m_pipes.end())
        {
            RT(READ, "%llx @ %lld[%lld] -> not found", inode, offset, *count);
            *count = 0;
            return NOT_FOUND;
        } else {
            Pipe & p = static_cast<Pipe &>(*vf_p->second);
            retval = p.read(buf, (size_t) *count, (size_t) offset);
        }
    } else {
        assert(m_files[inode]->isFile());
        RT(READ, "%llx @ %lld[%lld]", inode, offset, *count);

        File & f = static_cast<File &>(*vf_p->second);
        retval = f.read(buf, (size_t) *count, (size_t) offset);
    }
    if (retval > 0) {
        *count = (uint64_t) retval;
        retval = OK;
    } else {
        *count = 0;
    }

    return (int) retval;
}

int FilesystemServer::write(file_id_t inode, uint64_t offset, const char *buf, uint64_t *count)
{
    OP_COUNTER(WRITE);
    RT(WRITE, "%llx @ %lld[%lld]", inode, offset, *count);
    auto vf_p = m_files.find(inode);
    ssize_t retval;
    if(vf_p == m_files.end())
    {
        vf_p = m_pipes.find(inode);
        if(vf_p == m_pipes.end())
            return NOT_FOUND;
        else
        {
            Pipe & p = static_cast<Pipe &>(*vf_p->second);
            retval = p.write(buf, (size_t) *count, (size_t) offset);
        }
    } else {
        File & f = static_cast<File &>(*vf_p->second);
        retval = f.write(buf, (size_t) *count, (size_t) offset);
    }

    if (retval > 0) {
        *count = (uint64_t) retval;
        retval = OK;
    } else {
        *count = 0;
    }

    return (int) retval;
}


int FilesystemServer::addmap(uintptr_t appId, Inode parent, const char *path, InodeType child, bool overwrite, InodeType *exist)
{
    OP_COUNTER(ADDMAP);
    RT(ADDMAP, "[%d] %llx(@%d):%s -> %llx(@%d)", m_id, parent.id, parent.server, path, child.id, child.server);
    int ret = OK;

    if(strcmp(path, "stats") == 0) update_stat_file(InodeOnly(child));

    auto p_i = m_files.find(parent.id);

    // lazy allocate a parent directory
    if(p_i == m_files.end())
    {
        Dir *lazyp = new Dir(true, 0777);
        lazyp->id(parent.id);
        m_files[parent.id] = lazyp;

        p_i = m_files.find(parent.id);
    }

    if(p_i == m_files.end()) return NOT_FOUND;

    Dir *p = (Dir *)p_i->second;

    auto c_i = p->find(path);
    if(c_i != p->end()) 
    {
        *exist = c_i->second->inode;
        ret = EXIST;
        if(!overwrite) // if it exists and overwrite flag is false, return now
            return ret;
        else
        {
#if DIRECTORY_CACHE
            appRemoveCache(appId, c_i->second);
#endif
            p->erase(c_i);
        }
    }

    // Add the directory entry
    p->addChild(path, child); // add the child to the parent

#if DIRECTORY_CACHE
    if(strcmp(path, "..") != 0)
        appAddCache(appId, p->find(path)->second);
#endif

    // To be more general always set *exist to the result of the map.
    if(ret != EXIST)
        *exist = child;

    RT(ADDMAP, "[%d] %llx(@%d):%s -> %llx(@%d) ok!", m_id, parent.id, parent.server, path, child.id, child.server);
    return ret;
}

int FilesystemServer::removemap(uintptr_t appId, file_id_t parent_inode, const char *path)
{
    OP_COUNTER(REMOVEMAP);
    RT(REMOVEMAP, "[%d] %llx:%s", m_id, parent_inode, path);

    if(strcmp(path, "stats") == 0) clear_stats();

    auto vf_i = m_files.find(parent_inode);
    if(vf_i == m_files.end()) return NOT_FOUND;
    if(!vf_i->second->isDir()) return NOT_DIR;
    Dir *p = (Dir *)vf_i->second;

    auto c = p->find(path);
    if(c == p->end()) return NOT_FOUND;

    // if we remove the map then we have to notify cachers...
#if DIRECTORY_CACHE
    if(strcmp(path, "..") != 0)
        appRemoveCache(appId, c->second);
#endif

    p->erase(c);
    return OK;
}

int FilesystemServer::fstatat(file_id_t inode, struct FauxStat64 *buf)
{
    OP_COUNTER(FSTAT);
    RT(FSTAT, "[%d] %llx", m_id, inode);
    auto vf_i = m_files.find(inode);
    if(vf_i == m_files.end()) return NOT_FOUND;

    VFile *vfile = vf_i->second;

    size_t size = vfile->size();

    buf->st_dev = 0;
    buf->st_ino = vfile->id();
    buf->st_mode = vfile->mode();
    buf->st_nlink = 0;
    buf->st_uid = vfile->uid();
    buf->st_gid = vfile->gid();
    buf->st_rdev = 0;
    buf->st_blksize = BufferCache::block_size;
    buf->st_blocks = (size / buf->st_blksize) + 1;
    buf->st_size = size;
    buf->_st_atime = vfile->atime();
    buf->_st_mtime = vfile->mtime();
    buf->_st_ctime = vfile->ctime();
    buf->st_mode |= vfile->type();

    RT(FSTAT, "[%d] %llx (size: %d) type: %d", m_id, vfile->id(), size, vfile->type());
    return OK;
}

int FilesystemServer::chmod(file_id_t inode, mode_t mode)
{
//    T("[%d] %llx (%x)", m_id, inode, mode);
    OP_COUNTER(CHMOD);
    RT(ALL, "%llx (%x)", inode, mode);
    auto vf_i = m_files.find(inode);
    if(vf_i == m_files.end()) return NOT_FOUND;
    VFile *vf_p = vf_i->second;

    vf_p->mode(mode);
    return OK;
}

int FilesystemServer::chown(file_id_t id_, uid_t uid, gid_t gid)
{
    OP_COUNTER(CHOWN);
    RT(ALL, "%llx (%d:%d)", id_, uid, gid);
    auto vf_i = m_files.find(id_);
    if(vf_i == m_files.end()) return NOT_FOUND;
    VFile *vf_p = vf_i->second;

    vf_p->uid(uid);
    vf_p->gid(gid);
    return OK;
}

int FilesystemServer::direntry(uintptr_t appId, file_id_t id_, int offset, uint64_t *size, char *buf, bool *more)
{
    OP_COUNTER(DIRENT);
    RT(DIRENTRY, "[%d] %llx @ (%d) size: %lld", m_id, id_, offset, *size);
    auto vf_i = m_files.find(id_);
    if(vf_i == m_files.end()) { *size = 0; *more = false; return NOT_FOUND; }
    auto vf_p = vf_i->second;
    assert(vf_p->isDir());

    Dir & p = static_cast<Dir &>(*vf_p);
    auto e = p.begin();

    unsigned int written = 0;
    unsigned int entries = 0;

    int i = 0;

    // special case the dot dir.
    if(! p.isVirtual())
    {
        if(offset == 0)
        {
            const char *name = ".";
            int namelen = 1;
            unsigned int reclen = 0;

            reclen = ALIGN(offsetof(struct FauxDirent, d_name) + namelen + 1,
                    sizeof(uint64_t));

            // we have filled the user provided buf so return it
            assert(written + reclen <= *size);
            FauxDirent *dest = (FauxDirent *)(buf + written);
            dest->d_ino = id_;

            // we use 0 for our root's ino, but libc thinks that means it is
            // deleted so set to something bogus.
            if(dest->d_ino == 0) dest->d_ino = 0xDEADBEEF;

            dest->d_type = p.type();
            entries++;
            dest->d_off = ++offset;
            dest->d_reclen = reclen;

            strcpy(dest->d_name, name);

            written += reclen;
        }
        i++;
    }

    for(; e != p.end(); i++, e++)
    {
        const char *key = e->first.c_str();

        if(i >= offset)
        {
            const char *name;
            int namelen = 0;
            unsigned int reclen = 0;

            RT(ALL, "[%d] %llx @ off: %d -> %s", m_id, id_, entries, key);

            name = key;

            namelen = strlen(name);
            reclen = ALIGN(offsetof(struct FauxDirent, d_name) + namelen + 1,
                    sizeof(uint64_t));

            // we have filled the user provided buf so return it
            if(written + reclen > *size) break;

            // otherwise fill in the struct
            FauxDirent *dest = (FauxDirent *)(buf + written);

            dest->d_ino = e->second->inode.id;

            // we use 0 for our root's ino, but libc thinks that means it is
            // deleted so set to something bogus.
            if(dest->d_ino == 0) dest->d_ino = 0xDEADBEEF;

            if(e->second->isDir()) dest->d_type = e->second->isShared() ? DT_BLK : DT_DIR;
            else if(e->second->isFile()) dest->d_type = DT_REG;
            else if(e->second->isSymlink()) dest->d_type = DT_LNK;
            else assert(false);

            entries++;
            dest->d_off = ++offset;
            dest->d_reclen = reclen;

            strcpy(dest->d_name, name);

            written += reclen;

#if DIRECTORY_CACHE
            // since we give it to the app, remember in case we have to
            // remove it from the cache
            if((dest->d_type == DT_DIR || dest->d_type == DT_BLK) && strcmp(name, "..") != 0)
                appAddCache(appId, e->second);
#endif
        }
    }

    *size = written;
    *more = (e != p.end());

    return written ? entries : -1;
}

int FilesystemServer::unlink(file_id_t id)
{
    OP_COUNTER(UNLINK);
    RT(UNLINK, "[%d] id: %llx", m_id, id);

    auto vf_i = m_files.find(id);
    if(vf_i == m_files.end()) return NOT_FOUND;
    VFile *c = vf_i->second;

    assert(c->isFile() || c->isSymlink());

    bool anonymous = false;
    for(auto fd_i = m_fds.begin(); fd_i != m_fds.end(); fd_i++)
    {
        if(fd_i->second->id() == c->id() && fd_i->second->count())
        {
            //T("[%s] refs: %d", vf_p->name().c_str(), fd_i->second->count());
            anonymous = true;
            break;
        }
    }

    if(anonymous)
    {
        char newname[MAX_FNAME_LEN];
        sprintf(newname, "f.%lld", rdtscll());
        InodeType child = { m_id, c->id(), c->type() };
        m_orphans.addChild(newname, child);
    } else {
        m_files.erase(vf_i);               // erase from server
        delete c;
    }

    return OK;
}

int FilesystemServer::rmdir(file_id_t id, int phase)
{
    OP_COUNTER(RMDIR);
    RT(RMDIR, "[%d] %llx", m_id, id);

    auto vf_i = m_files.find(id);
    if(vf_i == m_files.end())
    {
        Dir *lazyp = new Dir(true, 0777);
        lazyp->id(id);
        m_files[id] = lazyp;

        vf_i = m_files.find(id);
    }
    VFile *c = vf_i->second;
    if(! c->isDir()) return NOT_DIR;

    auto c_d = static_cast<Dir *>(c);
    for(auto c_i = c_d->children().begin(); c_i != c_d->children().end(); c_i++)
    {
        if(strcmp(c_i->first.c_str(), "..") == 0) continue;
        return NOT_EMPTY;
    }

    if(phase == RMDIR_TRY)
    {
        if(c_d->locked())
            return -EAGAIN;
        assert(c_d->locked() == false);
        c_d->lock();
        return OK;
    }
    if(phase == RMDIR_FAIL)
    {
        assert(c_d->locked() == true);
        c_d->lock(false);
        return OK;
    }

    if(phase == RMDIR_SUCCESS)
    {
        assert(c_d->locked());
        c_d->lock(false);
    }

    bool anonymous = false;

    for(auto fd_i = m_fds.begin(); fd_i != m_fds.end(); fd_i++)
    {
        if(fd_i->second->id() == c->id() && fd_i->second->count())
        {
            RT(RMDIR, "removing open dir with refs: %d", fd_i->second->count());
            anonymous = true;
            break;
        }
    }

    RT(RMDIR, "removing: %llx", id);

    if(anonymous)
    {
        char newname[MAX_FNAME_LEN];
        sprintf(newname, "f.%llx", rdtscll());
        InodeType child = { m_id, c->id(), c->type() };
        m_orphans.addChild(newname, child);
    }
    else
    {
        m_files.erase(vf_i);    // erase from server
        delete c;
    }

    return OK;
}

int FilesystemServer::readlink(file_id_t inode, char *result, uint64_t *size)
{
    OP_COUNTER(READLINK);
    RT(SYMLINK, "%llx", inode);

    auto vf_i = m_files.find(inode);
    if(vf_i == m_files.end()) return NOT_FOUND;
    VFile *vfile = vf_i->second;

    assert(vfile->isSymlink());
    Symlink & l = static_cast<Symlink &>(*vfile);
    assert(l.size() < MAX_FNAME_LEN);

    strcpy(result, l.dest().c_str());
    *size = l.size();

    return OK;
}

int FilesystemServer::truncate(file_id_t inode, uint64_t len)
{
    RT(ALL, "%llx @ %lld", inode, len);
    OP_COUNTER(TRUNC);
    auto vf_i = m_files.find(inode);
    if(vf_i == m_files.end()) return NOT_FOUND;
    VFile *vfile = vf_i->second;

    if(unlikely(vfile->isSymlink())) return ISSYM;
    if(unlikely(vfile->isDir())) return ISDIR;

    assert(vfile->isFile());
    File* file = static_cast<File*>(vfile);
    file->truncate(len);

    return OK;
}

int FilesystemServer::resolve(uintptr_t appId, file_id_t parent_id, char *path, InodeType *exist)
{
    OP_COUNTER(RESOLVE);
    RT(RESOLVE, "[%d] %llx:%s", m_id, parent_id, path);

    assert(path[0]);

    // find the parent
    auto vf_i = m_files.find(parent_id);
    if(vf_i == m_files.end()) return RTOUT(RESOLVE,NOT_FOUND,"[%d] %llx:%s -> NOT_FOUND", m_id, parent_id, path);

    // make sure parent is dir
    if(!vf_i->second->isDir()) return RTOUT(RESOLVE,INVALID,"[%d] %llx:%s -> NOT_FOUND", m_id, parent_id, path);

    Dir *p = static_cast<Dir *>(vf_i->second);

    // find the child
    auto c_i = p->find(path);
    if(c_i == p->end()) return RTOUT(RESOLVE,NOT_FOUND,"[%d] %llx:%s -> NOT_FOUND", m_id, parent_id, path);

    VFileLoc *vfl_p = c_i->second;
    *exist = vfl_p->inode;

#if DIRECTORY_CACHE
    // if we resolve this for the app, then we need to keep track
    // in case the value changes
    appAddCache(appId, vfl_p);
#endif

    RT(RESOLVE, "[%d] %llx:%s -> %llx(@%d) type: %d (%s)", m_id, parent_id, path, exist->id, exist->server, exist->type, path);
    return OK;
}

// FIXME: this doesn't really obey the POSIX semantic since updates to the
// times should be neglected whenever the caller doesn't have write permissions
// on one of the directory in the filename -- siro
int FilesystemServer::utimens(file_id_t inode, struct timespec *atime, struct timespec *mtime, int flags)
{
    OP_COUNTER(UTIMENS);
    RT(ALL, "%llx", inode);

    auto vf_i = m_files.find(inode);
    if(vf_i == m_files.end()) return NOT_FOUND;
    VFile *vfile = vf_i->second;

    // if we have the AT_SYMLINK_NOFOLLOW flag then we simply
    // want to set the utimens on the symlink, otherwise we
    // want to set it on the file that the symlink points to
    // returning ISSYM allows the client to do the lookup
    // and make the utimens call on the dest.
    if (vfile->isSymlink() && !(flags & AT_SYMLINK_NOFOLLOW))
        return ISSYM;

    if (atime->tv_nsec != UTIME_OMIT)
        vfile->atime(*atime);
    if (mtime->tv_nsec != UTIME_OMIT)
        vfile->mtime(*mtime);

    return 0;
}

int FilesystemServer::symlink(file_id_t parent_inode, char* src, const char* dest)
{
    OP_COUNTER(SYMLINK);
    RT(SYMLINK, "[%d] %llx:%s -> %s", m_id, parent_inode, src, dest);

    auto p_i = m_files.find(parent_inode);

    // lazy allocate a parent directory
    if(p_i == m_files.end())
    {
        Dir *lazyp = new Dir(true, 0777);
        //FIXME: this doesn't set the server on the shadow parent dir.
        lazyp->id(parent_inode);
        m_files[parent_inode] = lazyp;

        p_i = m_files.find(parent_inode);
    }

    if(p_i == m_files.end()) return NOT_FOUND;
    if(!p_i->second->isDir()) return NOT_DIR;

    Dir *np = (Dir *)p_i->second;

    if (np->find(src) != np->end()) return EXIST;

    VFile* slink = new Symlink(dest);
    assert(slink);
    m_files[slink->id()] = slink;
    InodeType symlink_inode = { m_id, slink->id(), slink->type() };
    np->addChild(src, symlink_inode);

    return OK;
}

// FD Management
int FilesystemServer::close(fd_t fd, off64_t offset, uint64_t size)
{
    OP_COUNTER(CLOSE);
    RT(CLOSE, "%llx", fd);
    auto fd_i = m_fds.find(fd);
    assert(fd_i != m_fds.end());
    auto fd_p = fd_i->second;
    fd_p->remove();

    int count = fd_p->count();

    assert(count >= 0);

    if(count == 0)
    {
        file_id_t inode = fd_p->id();
        auto vf_i = m_files.find(inode);
        if(vf_i == m_files.end())
        {
            vf_i = m_pipes.find(inode);
            assert(vf_i != m_pipes.end());

            auto vf_p = vf_i->second;

            Pipe & p = static_cast<Pipe &>(*vf_p);
            if(p.readFd() == fd) p.closeRead();
            if(p.writeFd() == fd) p.closeWrite();
            if(p.readFd() == 0 && p.writeFd() == 0)
            {
                m_pipes.erase(vf_i);
                delete vf_p;
            }
        } else {
            assert(vf_i != m_files.end());
            auto vf_p = vf_i->second;

            // See if the file was orphaned while it was open
            auto o_i = m_orphans.find(vf_p->id());
            if(o_i != m_orphans.end())
            {
                // now that there are no longer any fd's pointing to this node it
                // is no longer reachable, so remove it from the orphans
                m_orphans.erase(o_i);
                m_files.erase(vf_i); // erase from server
                delete vf_p;
            } else {
                if(size != UINT64_MAX) {
                    auto & file = static_cast<File &>(*vf_p);
                    file.size(size);
                }
            }
        }

        // Erase it from the fd list
        m_fds.erase(fd_i);
        delete fd_p;
    }
    else
    {
        if(offset != -1)
            fd_p->offset(offset);

        if(size != UINT64_MAX) {
            auto & file = static_cast<File &>(*m_files.find(fd_p->id())->second);
            file.size(size);
        }
    }
    return OK;
}

int FilesystemServer::dup(fd_t fd, Inode& inode, int& type, uint64_t& size)
{
    OP_COUNTER(DUP);
    auto it = m_fds.find(fd);
    if (it == m_fds.end()) return INVALID;
    auto fd_p = it->second;

    auto vf_i = m_files.find(fd_p->id());
    if(vf_i == m_files.end())
    {
        vf_i = m_pipes.find(fd_p->id());
        if(vf_i == m_pipes.end())
            return NOT_FOUND;
    }

    auto & vf = *vf_i->second;
    inode.server = m_id;
    inode.id = vf.id();
    size = vf.size();
    type = vf.type();

    m_fds[fd]->add();
    RT(DUP,"%llx (%s) new count: %d", fd, "name" /*m_fds[fd]->name().c_str()*/, m_fds[fd]->count());
    return OK;
}

int FilesystemServer::read(fd_t fd, char *buf, uint64_t *count)
{
    assert(m_fds.find(fd) != m_fds.end());
    auto *fd_p = m_fds[fd];

    uint64_t old_offset = fd_p->offset();
    int ret = read(fd_p->id(), old_offset, buf, count);
    fd_p->offset(old_offset + *count);
    return ret;
}

int FilesystemServer::write(fd_t fd, const char *buf, uint64_t *count)
{
    assert(m_fds.find(fd) != m_fds.end());
    auto *fd_p = m_fds[fd];
    RT(WRITE, "fd: %llx @ %lld[%lld]", fd, fd_p->offset(), *count);

    uint64_t old_offset = fd_p->offset();
    int ret = write(fd_p->id(), old_offset, buf, count);
    fd_p->offset(old_offset + *count);
    return ret;
}

int FilesystemServer::lseek(fd_t fd, uint64_t& offset, int whence)
{
    OP_COUNTER(LSEEK);
    assert(m_fds.find(fd) != m_fds.end());
    auto *fd_p = m_fds[fd];

    auto vf_i = m_files.find(fd_p->id());
    assert(vf_i != m_files.end()); //FIXME
    assert(vf_i->second->isFile());
    auto & vf = static_cast<File &>(*vf_i->second);

    auto size = vf.size();

    uint64_t old_offset = fd_p->offset();
    switch(whence)
    {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            offset += old_offset;
            break;
        case SEEK_END:
            offset = size + offset;
            break;
#ifdef SEEK_DATA
        case SEEK_DATA: // hare doesn't have holes...? always return data
            offset = old_offset;
            break;
        case SEEK_HOLE:
            offset = size; // implicit hole at end
            break;
#endif
        default:
            assert(false);
    }

    // set it internally
    fd_p->offset(offset);

    return OK;
}

int FilesystemServer::blocks(file_id_t inode, uintptr_t* blocklist, uint64_t offset, uint64_t count)
{
    OP_COUNTER(BLOCKS);
    auto vf_i = m_files.find(inode);
    assert(vf_i != m_files.end());
    assert(vf_i->second->isFile());
    auto & file = static_cast<File &>(*vf_i->second);
    int retval = file.blocks(blocklist, offset, count);
    if(retval != OK)
    {
        T("[%d] oops ran out of memory allocating blocks (offset: %lld, count: %lld, free: %lld)", m_id, offset, count, m_buffer_cache->free());
    }
    return retval;
}

int FilesystemServer::flush(fd_t fd, uint64_t size, off64_t offset)
{
    RT(FLUSH, "fd: %llx @%lld/%lld", fd, offset, size);
    OP_COUNTER(FLUSH);
    auto it = m_fds.find(fd);
    if (it == m_fds.end()) return INVALID;
    auto fd_p = it->second;
    fd_p->offset(offset);

    auto & file = static_cast<File &>(*m_files.find(fd_p->id())->second);
    file.size(size);
    return OK;
}

// Statistics Gathering
void FilesystemServer::update_stat_file(Inode &file_ino)
{
    auto file_i = m_files.find(file_ino.id);
    assert(file_i != m_files.end());
    auto & file = static_cast<File &>(*file_i->second);

    std::ostringstream os;

#if DBG_SHOW_OPS
    char * opnames[] = {
        "CREATE   ",
        "DESTROY  ",
        "ADD_MAP  ",
        "RM_MAP   ",
        "PIPE     ",
        "OPEN     ",
        "READ     ",
        "WRITE    ",
        "LSEEK    ",
        "CLOSE    ",
        "DUP      ",
        "FSTAT    ",
        "DIRENT   ",
        "UNLINK   ",
        "READLINK ",
        "RMDIR    ",
        "TRUNC    ",
        "CHMOD    ",
        "CHOWN    ",
        "RESOLVE  ",
        "UTIMENS  ",
        "SYMLINK  ",
        "BLOCKS   ",
        "FLUSH    " };

    os << "------------------------------------------------------------" << endl;
    os << "Ops:" << endl;
    os << "Name     " << setw(10) << "Count" << setw(16) << "Time (us)" << setw(16) << "Avg (cycle/op)" <<endl;

    uint64_t count_total = 0;
    uint64_t time_total = 0;
    uint64_t cycles_total = 0;

    for(auto i = 0; i < NUM_FS_TYPES; i++)
    {
        auto count = m_op_count[i];
        auto cycles = m_op_times[i];
        auto time = ts_to_us(cycles);
        auto avg = count > 0 ? cycles / count : 0;
        os << opnames[i] << setw(10) << count << setw(16) << time << setw(16) << avg <<endl;
        count_total += count;
        time_total += time;
        cycles_total += cycles;
    }

    os << "------------------------------------------------------------" << endl;
    os << "TOTAL    " << setw(10) << count_total << setw(16) << time_total << setw(16) << cycles_total / count_total <<endl;

#endif

#if DBG_SHOW_FDS
    os << "------------------------------------------------------------" << endl;
    os << "File Descriptors:" << endl;
    os << setw(12) << "FD"
       << setw(12) << "INODE"
       << setw(8) << "COUNT"
       << setw(16) << "NAME"
       << endl;
    for(auto fd_i = m_fds.begin(); fd_i != m_fds.end(); fd_i++)
        os << setw(12) << std::hex << fd_i->second->fd()      <<
              setw(12) << std::hex << fd_i->second->id()      <<
              setw(8)  << std::dec << fd_i->second->count()   <<
              setw(16) << fd_i->second->name()                <<
              setw(16) << "" <<
              endl;
    os << "------------------------------------------------------------" << endl;
#endif

#if DBG_SHOW_ORPHANS
    os << "orphans: " << m_orphans.children().size() << endl;
    for(auto i = m_orphans.begin(); i != m_orphans.end(); i++)
    {
        os << "\t" << i->first.c_str() << endl;
    }
#endif

#if DBG_SHOW_BUFFERCACHE
    uint64_t used = m_buffer_cache->used();
    uint64_t free = m_buffer_cache->free();
    uint64_t total = used + free;

    os << "------------------------------------------------------------" << endl;
    os << "Buffer Cache:" << setw(12) << "used bytes" << "         " << setw(12) << "free bytes" << endl;
    std::locale comma_locale(std::locale(), new comma_numpunct());
    os.imbue(comma_locale);
    os << "             ";
    os << std::fixed << setw(12) << used <<
        " (" << setw(3) << used * 100 / total << "%) ";
    os << std::fixed << setw(12) << free <<
        " (" << setw(3) << free * 100 / total << "%)" << endl;
    os << "------------------------------------------------------------" << endl;
#endif

#if DBG_SHOW_QUEUE_DELAY
    os << "------------------------------------------------------------" << endl;
    os << "avg queue delay:" << setw(12) << g_queue_time / g_msg_count << endl;
    os << "total queue delay (cycles):" << g_queue_time << endl;
    os << "total queue delay (ms):" << ts_to_ms(g_queue_time) << endl;
    os << "total msgs:" << g_msg_count << endl;
    os << "------------------------------------------------------------" << endl;
#endif

#if ENABLE_DBG_CHECK_QUEUES
    os << "Queues:" << endl;
    os << "times checked:   " << g_times_checked << endl;
    os << "times_not_empty: " << g_times_not_empty << endl;
    os << "perce_not_empty: " << 100 * g_times_not_empty / (double)g_times_checked << endl;
    os << "times_not_total: " << g_times_total << endl;
#endif

    file.truncate(os.str().size());
    file.write(os.str().c_str(), os.str().size(), 0);
}

void FilesystemServer::clear_stats()
{
    for(auto i = 0; i < NUM_FS_TYPES; i++)
    {
        m_op_count[i] = 0;
        m_op_times[i] = 0;
    }
}

void FilesystemServer::idle()
{
    return;
    static uint64_t time_since_last;
    uint64_t now = rdtsc64();
    if(now - time_since_last > 5000000000)
    {
        time_since_last = now;
    }
}

// Op functions
void FilesystemServer::opStart(const OpCounter &c)
{
    m_op_count[c.id()]++;
}

void FilesystemServer::opEnd(const OpCounter &c)
{
    int64_t elapsed_cycles = rdtscll() - c.start();
    if(elapsed_cycles > 0)
        m_op_times[c.id()] += elapsed_cycles;
}

// OpCounter Class

OpCounter::OpCounter(int opid)
: m_id(opid)
, m_start(rdtscll())
{
    FilesystemServer::fs().opStart(*this);
}

OpCounter::~OpCounter()
{
    FilesystemServer::fs().opEnd(*this);
}



