#include <stddef.h>
#include <dirent.h>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include <messaging/dispatch.h>
#include <sys/fs/fs.h>
#include <sys/fs/lib.h>
#include "dir_cache.hpp"

#define ALIGN(x, a)            ALIGN_MASK(x, (typeof(x))(a) - 1)
#define ALIGN_MASK(x, mask)    (((x) + (mask)) & ~(mask))

#include "dir.hpp"

extern DispatchTransaction fosDirentReq(InodeType inode, off64_t offset, uint64_t size);
extern int fosDirentWaitReply(DispatchTransaction trans, uint64_t *size, bool *more, void (*_callback)(void *, uint64_t, void *), void *p);

using namespace fos;
using namespace fs;
using namespace app;

int fosFreeDir(void *dirp)
{
    Dir *p = (Dir *)dirp;
    assert(p->isDir());
    p->close();
    delete p;
    return OK;
}

int fosGetDents64(void *vp, struct linux_dirent64 *dentp, uint64_t count)
{
    return ((Dir*)vp)->getdents64(dentp, count);
}

int fosGetDents(void *vp, struct linux_dirent *dentp, uint64_t count)
{
    return ((Dir*)vp)->getdents(dentp, count);
}

Dir::Dir(InodeType inode_, fd_t fd_, uint64_t offset_)
: VFile(inode_, fd_)
, m_offset(offset_)
, m_current_server(0)
{
    // we only ask our one server if it is shared
    if(!isShared())
        m_current_server = inode_.server;
}

Dir::~Dir()
{
#if ENABLE_DENT_SNAPSHOT
    for(auto i = m_dents.begin(); i != m_dents.end(); i++)
        free(*i);
    m_dents.clear();
#endif

}

int Dir::close()
{
    return fosFdClose(m_inode.server, m_fd, -1, UINT64_MAX);
}

int Dir::getdents64(struct linux_dirent64 *out_dents, uint64_t count)
{
#if ENABLE_DENT_SNAPSHOT
    if(CONFIG_FS_SERVER_COUNT > 1)
    {
        cache_dents();
        if(m_offset == m_dents.size()) return 0;
        int written = 0;
        int index = 0;
        for(auto i = m_dents.begin(); i != m_dents.end(); i++, index++)
        {
            if(index < m_offset) continue;
            uint64_t recsize = (*i)->d_reclen;
            if(written + recsize > count) break;
            memcpy(out_dents, *i, recsize);
            out_dents = (struct linux_dirent64 *)((char *)out_dents + recsize);
            written += recsize;
            m_offset++;
        }
        return written;
    }
#endif
    if(m_current_server == CONFIG_FS_SERVER_COUNT) return 0;
    int ret;
    bool more;
    uint64_t old_count = count;
dent_again:
    InodeType dir = { (int)m_current_server, m_inode.id, m_inode.type };
    ret = fosDirent(dir, m_offset, &count, out_dents, &more);

    int consumed = 0;
    struct FauxDirent *src_dent = (struct FauxDirent *)out_dents;

    while(consumed < count)
    {
        int namelen = strlen(src_dent->d_name);
        int reclen = ALIGN(offsetof(struct linux_dirent, d_name) + namelen + 2,
                sizeof(long));

        char*path = src_dent->d_name;

        if((src_dent->d_type == DT_DIR || src_dent->d_type == DT_BLK) && path[0] != '.')
        {
            // calculate server from child inode
            InodeType child = { (int)(src_dent->d_ino >> INODE_SHIFT), src_dent->d_ino, src_dent->d_type == DT_DIR ? TYPE_DIR : TYPE_DIR_SHARED };
            DirCache::dc().add(inode(), path, strlen(path), child);
        }

        // DT_BLK is overloaded as a "shared dir" but just keep it
        // as a regular dir as far as the application is concerned
        if(src_dent->d_type == DT_BLK) src_dent->d_type = DT_DIR;

        // advance the pointers / accumulators
        consumed += src_dent->d_reclen;
        src_dent = (struct FauxDirent *)((char *)src_dent + src_dent->d_reclen);
    }


    if(!isShared())
    {
        assert(ret != -2);
        if(ret == -1)
        {
            m_current_server == CONFIG_FS_SERVER_COUNT;
            return 0;
        }
    }

    // -1 indicates no more entries
    // -2 indicates "not found" which means that this server doesn't have any
    // directory entries for this directory
    if(ret == -1 || ret == -2)
    {
        m_current_server++;
        if(m_current_server == CONFIG_FS_SERVER_COUNT) return 0;
        count = old_count;
        m_offset = 0;
        goto dent_again;
    }

    // <0 and not -1 indicates not found or some actual error
    if(ret < 0) return -1;

    // if we get here the server provided some directory entries. If the server also
    // replied that there are no more entries then we can move the scan to the
    // next server
    if(!more && m_current_server < CONFIG_FS_SERVER_COUNT)
    {
        if(isShared())
        {
            m_current_server++;
            m_offset = 0;
        } else {
            m_current_server = CONFIG_FS_SERVER_COUNT; // if we're not distributing directories
            // then if there are no more entries from this server then there are no more in
            // total.
        }
    }
    else
    // otherwise, ret is the number of entries
    {
        m_offset += ret;
    }

    return count;
}

int Dir::getdents(struct linux_dirent *out_dent, uint64_t count)
{
#if ENABLE_DENT_SNAPSHOT
    cache_dents();
#endif
    char buf[count];
    uint64_t return_count = count;

    if(m_current_server == CONFIG_FS_SERVER_COUNT) return 0;

    int ret;
    bool more;
dent_again:
    InodeType dir = { (int)m_current_server, m_inode.id, m_inode.type };
    ret = fosDirent(dir, m_offset, &return_count, buf, &more);

    if(!isShared())
    {
        assert(ret != -2);
        if(ret == -1)
        {
            m_current_server = CONFIG_FS_SERVER_COUNT;
            return 0;
        }
    }

    // -1 indicates no more entries
    // -2 indicates "not found" which means that this server doesn't have any
    // directory entries for this directory
    if(ret == -1 || ret == -2)
    {
        if(m_current_server == CONFIG_FS_SERVER_COUNT - 1) return 0;
        m_current_server++;
        m_offset = 0;
        return_count = count;
        goto dent_again;
    }
    if(ret < 0) return -1;


    int total = return_count;
    int written = 0;
    int consumed = 0;
    uint64_t old_offset = m_offset;

    struct FauxDirent *src_dent = (struct FauxDirent *)buf;
    struct linux_dirent *dest_dent = out_dent;

    while(consumed < total)
    {
        int namelen = strlen(src_dent->d_name);
        int reclen = ALIGN(offsetof(struct linux_dirent, d_name) + namelen + 2,
                sizeof(long));

        LT(DIR, "(%llx) @ %lld -> %s", m_inode.id, m_offset, src_dent->d_name);

        if(written + reclen > count)
            break;

        dest_dent->d_ino = src_dent->d_ino;
        dest_dent->d_off = src_dent->d_off;
        dest_dent->d_reclen = reclen;
        strcpy(dest_dent->d_name, src_dent->d_name);

        // 32-bit getdents call will put the type as the last character of the
        // reclen (which will possibly be aligned to a given length)

        char *d_type = (char *)dest_dent + reclen - 1;
        *d_type = src_dent->d_type;

        // advance the pointers / accumulators
        m_offset++;
        consumed += src_dent->d_reclen;
        written += reclen;
        dest_dent = (struct linux_dirent *)((char *)dest_dent + reclen);
        src_dent = (struct FauxDirent *)((char *)src_dent + src_dent->d_reclen);
    }

    // if we get here the server provided some directory entries. If the server also
    // replied that there are no more entries then we can move the scan to the
    // next server
    //
    // note also that we hopefully consumed all of the directory entries, otherwise
    // we will need to return to the same server to get the remainder
    if(!more && m_current_server < CONFIG_FS_SERVER_COUNT -1 && (m_offset - old_offset == ret))
    {
        if(isShared())
        {
            m_current_server++;
            m_offset = 0;
        } else {
            m_current_server = CONFIG_FS_SERVER_COUNT; // if we're not distributing directories
            // then if there are no more entries from this server then there are no more in
            // total.
        }
    }

    return written;
}

int Dir::adddents(void *data, uint64_t size, int server)
{
    int consumed = 0;
    int count = 0;
    struct FauxDirent *src_dent = (struct FauxDirent *)data;

    while(consumed < size)
    {
        int namelen = strlen(src_dent->d_name);
        int reclen = ALIGN(offsetof(struct linux_dirent, d_name) + namelen + 2,
                sizeof(long));

        struct FauxDirent *newdent = (struct FauxDirent *)malloc(src_dent->d_reclen);

        memcpy(newdent, src_dent, src_dent->d_reclen);
        m_dents.push_back(newdent);

        char*path = src_dent->d_name;

        // DT_BLK is overloaded as a "shared dir" but just keep it
        // as a regular dir as far as the application is concerned
        if(newdent->d_type == DT_BLK) newdent->d_type = DT_DIR;

        if((src_dent->d_type == DT_DIR || src_dent->d_type == DT_BLK) && path[0] != '.')
        {
            // calculate server from child inode
            InodeType child = { (int)(src_dent->d_ino >> INODE_SHIFT), src_dent->d_ino, src_dent->d_type == DT_DIR ? TYPE_DIR : TYPE_DIR_SHARED };
            DirCache::dc().add(inode(), path, strlen(path), child);
        }

        // advance the pointers / accumulators
        consumed += src_dent->d_reclen;
        src_dent = (struct FauxDirent *)((char *)src_dent + src_dent->d_reclen);
        count++;
    }
    return count;
}

struct dent_callback_args
{
    void *dirobj;
    uint64_t *offset;
    int server;
};

static void _dirent_callback(void *buf, uint64_t size, void *p)
{
    struct dent_callback_args *args = (struct dent_callback_args *)p;
    Dir *d = (Dir *)args->dirobj;
    *(args->offset) += d->adddents(buf, size, args->server);
}

void Dir::cache_dents()
{
    // only cache the snapshot once...
    if(m_dents.size()) return;

    int csc = CONFIG_FS_SERVER_COUNT;
    int size_per_server = 49152; /* yes this is a computer number, it is 75% of a typical channel buffer */;  
    uint64_t actual_size;
    bool more[csc];
    uint64_t offsets[csc];
    bool one_has_more = true;
    for(int i = 0; i < csc; i++)
    {
        more[i] = isShared() ? true : (i == m_current_server);
        offsets[i] = 0;
    }

    struct dent_callback_args args;
    args.dirobj = this;

    DispatchTransaction dent_trans[csc];
    for(int i = 0; i < csc; i++)
    {
        InodeType dir = { i, m_inode.id, m_inode.type };
        if(isShared())
            dent_trans[i] = fosDirentReq(dir, offsets[i], size_per_server);
        else
            if(i == m_current_server)
                dent_trans[i] = fosDirentReq(dir, offsets[i], size_per_server);
    }

    while(one_has_more)
    {
        one_has_more = false;
        for(int i = 0; i < csc; i++)
        {
            if(!more[i]) continue;
            args.offset = &offsets[i];
            args.server = i;
            int ret = fosDirentWaitReply(dent_trans[i], &actual_size, &more[i], _dirent_callback, &args);
            if(more[i])
            {
                InodeType dir = { i, m_inode.id, m_inode.type };
                dent_trans[i] = fosDirentReq(dir, offsets[i], size_per_server);
                one_has_more = true;
            }
        }
    }
}
