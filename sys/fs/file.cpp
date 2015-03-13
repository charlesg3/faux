#include <algorithm>
#include <cassert>
#include <cerrno>
#include <ctime>
#include <list>
#include <memory>
#include <vector>

#include <config/config.h>

#include <messaging/dispatch.h>
#include <messaging/channel.h>
#include <name/name.h>
#include <sys/fs/fs.h>

#include "server.hpp"
#include "buffer_cache.hpp"

using namespace fos;
using namespace fs;
using namespace std;

static int g_inode_seq = 0;


VFile::VFile(FileType type_, mode_t mode_)
: m_id(g_inode_seq++)
, m_type(type_)
, m_mode(mode_)
, m_uid(0)
, m_gid(0)
{
    struct timespec now;
    clock_gettime(CLOCK_REALTIME_COARSE, &now);
    m_ctime = m_atime = m_mtime = now;

    // make the inodes globally unique
    if(FilesystemServer::id)
    {
        m_id &= INODE_MASK;
        m_id |= (FilesystemServer::id << INODE_SHIFT);
        // handle conflicts...
/*
        while(fs.find(m_inode) != fs.end())
        {
            m_inode = g_inode_seq++;
            m_inode &= INODE_MASK;
            m_inode |= (server_id << 28);
        }
        */
    }
}

VFile::~VFile()
{
}

// File
File::File(shared_ptr<BufferCache> buffer_cache, mode_t mode_)
: VFile(TYPE_FILE, mode_), m_buffer_cache(buffer_cache), m_size(0)
{
}

File::~File()
{
    list<uintptr_t> memory_blocks {begin(m_memory_blocks), end(m_memory_blocks)};
    m_buffer_cache->put(move(memory_blocks));
    m_memory_blocks.clear();

    list<uintptr_t> orphan_blocks {begin(m_orphan_blocks), end(m_orphan_blocks)};
    m_buffer_cache->put(move(orphan_blocks));
    m_orphan_blocks.clear();
}

int File::shrink(size_t new_size)
{
    size_t memory_blocks_nr = new_size / BufferCache::block_size;
    if ((new_size & (BufferCache::block_size - 1)) != 0)
        ++memory_blocks_nr;
    if ((m_memory_blocks.size() - memory_blocks_nr) == 0)
        return 0;
    list<uintptr_t> memory_blocks {begin(m_memory_blocks) + memory_blocks_nr, end(m_memory_blocks)};
    m_memory_blocks.resize(memory_blocks_nr);
    move(begin(memory_blocks), end(memory_blocks), back_inserter(m_orphan_blocks));
    return 0;
}

int File::grow(size_t new_size)
{
    size_t memory_blocks_nr = new_size / BufferCache::block_size;
    if ((new_size & (BufferCache::block_size - 1)) != 0)
        ++memory_blocks_nr;
    size_t memory_blocks_nr_diff = memory_blocks_nr - m_memory_blocks.size();
    if (memory_blocks_nr_diff == 0)
        return 0;
    auto memory_blocks = m_buffer_cache->get(memory_blocks_nr_diff);
    if (memory_blocks.size() != memory_blocks_nr_diff)
        return -ENOSPC;
    move(begin(memory_blocks), end(memory_blocks), back_inserter(m_memory_blocks));
    return 0;
}

void File::zero(size_t new_size)
{
    size_t memory_blocks_nr = m_size / BufferCache::block_size;
    size_t memory_block_offset = m_size & (BufferCache::block_size - 1);
    if (memory_block_offset != 0)
        ++memory_blocks_nr;
    size_t new_memory_blocks_nr = new_size / BufferCache::block_size;
    size_t new_memory_block_offset = new_size & (BufferCache::block_size - 1);
    if (new_memory_block_offset != 0)
        ++new_memory_blocks_nr;
    {
        const auto& memory_block = m_memory_blocks[memory_blocks_nr - 1];
        memset((void*) (memory_block + memory_block_offset), 0, new_size - m_size);
    }
#if 1
    for_each(begin(m_memory_blocks) + memory_blocks_nr, end(m_memory_blocks),
            [](const uintptr_t& memory_block) { memset((void*) memory_block, 0, BufferCache::block_size); });
#else
    for (auto memory_block = begin(m_memory_blocks) + memory_blocks_nr; memory_block != end(m_memory_blocks); ++memory_block)
        memset((void*) *memory_block, 0, BufferCache::block_size);
#endif
}

ssize_t File::read(void* buf, size_t count, size_t offset)
{
    ssize_t retval = 0;

    if (offset < m_size) {
        if (offset + count > m_size)
            count = m_size - offset;

        while ((size_t) retval < count) {
            size_t memory_block_offset = offset & (BufferCache::block_size - 1);
            size_t memory_block_nr = offset / BufferCache::block_size;
            size_t n = min(BufferCache::block_size - memory_block_offset, count - (size_t) retval);
            auto memory_block = m_memory_blocks[memory_block_nr];
            memcpy(buf, (void*) (memory_block + memory_block_offset), n);
            buf = (void*) ((uintptr_t) buf + n);
            offset += n;
            retval += n;
        }

        assert((size_t) retval == count); // sanity_check
    }

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    atime(now);

    return retval;
}

ssize_t File::write(const void* buf, size_t count, size_t offset)
{
    if (count == 0)
        return 0;

    size_t new_size = offset + count;
    size_t retval = 0;
    if (m_size < new_size) {
        retval = (ssize_t) grow(new_size);
        m_size = new_size;
    }
    if (retval)
        return retval;

    while ((size_t) retval < count) {
        size_t memory_block_offset = offset & (BufferCache::block_size - 1);
        size_t memory_block_nr = offset / BufferCache::block_size;
        size_t n = min(BufferCache::block_size - memory_block_offset, count - (size_t) retval);
        const auto& memory_block = m_memory_blocks[memory_block_nr];
        memcpy((void*) (memory_block + memory_block_offset), buf, n);
        buf = (void*) ((uintptr_t) buf + n);
        offset += n;
        retval += n;
    }

    assert((size_t) retval == count); // sanity check

    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    atime(now);
    mtime(now);

    return retval;
}

int File::blocks(uintptr_t *blocklist, int offset, int count)
{
    int index = 0;
    for_each(begin(m_memory_blocks) + offset, end(m_memory_blocks), [=,&index](const uintptr_t& memory_block) 
    {
        if(index < count)
            blocklist[index++] = memory_block - m_buffer_cache->start();
    });

    if(index < count)
    {
        offset -= (m_memory_blocks.size());
        for_each(begin(m_orphan_blocks) + offset, end(m_orphan_blocks), [=,&index](const uintptr_t& memory_block) 
        {
            if(index < count)
                blocklist[index++] = memory_block - m_buffer_cache->start();
        });
    }

    if(index < count)
        return NO_MEMORY;

    return OK;
}

size_t File::capacity()
{
    size_t cap = 0;
    for_each(begin(m_orphan_blocks), end(m_orphan_blocks), [=,&cap](const uintptr_t& memory_block) 
    {
        cap += BufferCache::block_size;
    });

    for_each(begin(m_memory_blocks), end(m_memory_blocks), [=,&cap](const uintptr_t& memory_block) 
    {
        cap += BufferCache::block_size;
    });

    return cap;
}

int File::truncate(size_t new_size)
{
    if (m_size < new_size) {
        int retval = grow(new_size);
        if (retval)
            return retval;
        // zero(new_size);
    } else if (m_size > new_size) {
        int retval = 0;
/*
        size_t offset = new_size;
        size_t count = capacity() - new_size;
        while ((size_t) retval < count) {
            size_t memory_block_offset = offset & (BufferCache::block_size - 1);
            size_t memory_block_nr = offset / BufferCache::block_size;
            size_t n = min(BufferCache::block_size - memory_block_offset, count - (size_t) retval);
            const auto& memory_block = m_memory_blocks[memory_block_nr];
            memset((void*) (memory_block + memory_block_offset), 0, n);
            offset += n;
            retval += n;
        }
        */

        retval = shrink(new_size);
        if (retval)
            return retval;
    }
    m_size = new_size;
    return 0;
}

// Directory
Dir::Dir(bool vdir_, mode_t mode_)
#if DIRECTORY_DISTRIBUTION
: VFile(((mode_ & S_ISSHR) || vdir_ ) ? TYPE_DIR_SHARED : TYPE_DIR, mode_ & ~(S_ISSHR))
#else
: VFile(TYPE_DIR, mode_ & ~(S_ISSHR))
#endif
, m_vdir(vdir_)
, m_locked(false)
{
}

Dir::~Dir()
{
    while(m_children.size())
        erase(m_children.begin());
}

void Dir::addChild(const std::string & filename, InodeType inode_)
{
    // check for leaks
    //auto vfl_i = m_children.find(filename);
    //if(vfl_i != m_children.end())
    //    T("leaking: %s", filename.c_str());
    //assert(vfl_i == m_children.end());

    m_children[filename] = new VFileLoc(inode_);
}

void Dir::erase(ChildMap::iterator needle)
{
    VFileLoc *vfl_p = needle->second;
    m_children.erase(needle);
    delete vfl_p;
}

void Dir::erase(const std::string & needle)
{
    erase(m_children.find(needle));
}

ChildMap::iterator Dir::find(file_id_t needle)
{
    auto i = m_children.begin();
    for(; i != m_children.end(); i++)
        if(i->second->inode.id == needle) break;
    return i;
}


Symlink::Symlink(const string & dest_, mode_t mode_)
: VFile(TYPE_SYMLINK, mode_)
, m_dest(dest_)
{
}

// Pipe
Pipe::Pipe(mode_t mode_)
: VFile(TYPE_PIPE, mode_)
, m_size(0)
, m_waiters(0)
{
    condInitialize(&m_cv);

}

Pipe::~Pipe()
{
}

void Pipe::closeRead()
{
    m_read_fd = 0;
}

void Pipe::closeWrite()
{
    m_write_fd = 0;
#if 0
    if(m_waiters)
    {
        fprintf(stderr, "awoken: m_waiters: %d\n", m_waiters);
        condNotifyAll(&m_cv);
    }
#endif
}

ssize_t Pipe::write(const void* buf, size_t count, size_t offset)
{
    assert(m_size >= 0);
    m_size += count;
#if 0
    if(count && m_waiters)
        condNotify(&m_cv);
#endif

    return count;
}

ssize_t Pipe::read(void* buf, size_t count, size_t offset)
{
    assert(m_size >= 0);
    if(m_size)
    {
        if(count > m_size)
            count = m_size;

        m_size -= count;
        return count;
    }

    if(m_write_fd)
    {
        return -EAGAIN;
        m_waiters++;
        condWait(&m_cv);
        m_waiters--;

        if(m_size)
        {
            if(count > m_size)
                count = m_size;

            m_size -= count;
            if(m_size && m_waiters)
                condNotify(&m_cv);

            return count;
        }
        else
            return 0;
    }
    else
        return 0;
}

