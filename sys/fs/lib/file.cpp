#include <algorithm>
#include <cassert>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>

#include <sys/fs/fs.h>
#include <sys/fs/lib.h>
#include <sys/fs/interface.h> // for return values?
#include <sys/fs/buffer_cache_log.h> // for return values?
#include "../buffer_cache.hpp"

#include "file.hpp"

namespace fos { namespace fs { namespace app {

File::File(InodeType inode_, fd_t fd_)
: VFile(inode_, fd_)
, m_blocks()
, m_size(0)
, m_state(CACHE_INVALID)
, m_offset(0)
{
}

File::~File()
{
}

uint64_t File::read(void *buf, uint64_t count)
{
    char *p = (char *)buf;
    uint64_t read_count = 0;

    if(m_state == CACHE_SHARED) m_state = CACHE_EXCLUSIVE;

#if ENABLE_BUFFER_CACHE
    if(m_state != CACHE_INVALID)
    {
        if (m_offset < m_size)
        {
            if (m_offset + count > m_size) count = m_size - m_offset;

            int block_size = BufferCache::block_size;

            while ((size_t) read_count < count) {
                size_t block_offset = m_offset & (block_size - 1);
                size_t block_nr = m_offset / block_size;
                size_t n = std::min((uint64_t)(block_size - block_offset), count - (size_t) read_count);
                auto block = m_blocks[block_nr];
                fosBufferCacheLog(BCL_ACCESS, inode().id, block);
                fosBufferCacheCopyOut(buf, block + block_offset, n);
                buf = (void*) ((uintptr_t) buf + n);
                m_offset += n;
                read_count += n;
            }

            assert((size_t) read_count == count); // sanity_check
        }
    }
    else // don't have it cached, go through server
#endif
        while(read_count < count)
        {
            uint64_t read_size = std::min(count - read_count, (uint64_t) FS_CHUNK_SIZE);
            int ret = fosFdRead(m_inode.server, m_fd, p + read_count, &read_size);

            if(ret != OK) return read_count ? read_count : ret;
            if(!read_size) break;

            read_count += read_size;
        }

    return read_count;
}

int File::poll()
{
    assert(false);
#if ENABLE_BUFFER_CACHE
    if(m_state != CACHE_INVALID)
    {
        if (m_offset < m_size)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
#endif
    {
        uint64_t read_size = 0;
        char buf[8];
        int ret = fosFdRead(m_inode.server, m_fd, buf, &read_size);
        if(ret != OK) return false;
        return true;
    }
}

uint64_t File::write(const void *buf, uint64_t count)
{
    char *p = (char *)buf;
    uint64_t written = 0;

    if(m_state == CACHE_SHARED) m_state = CACHE_EXCLUSIVE;

#if ENABLE_BUFFER_CACHE
    if(m_state != CACHE_INVALID)
    {
        int block_size = BufferCache::block_size;

        int left_in_block = block_size - (m_size % block_size);
        if(m_size % block_size == 0) left_in_block = 0;


        int cached_size = m_size;

        // FIXME: this doesn't chunk if it isn't big enough
        if(m_offset + count > (cached_size + left_in_block))
        {
            uint64_t additional_bytes = m_offset + count - left_in_block - cached_size;
            int additional_blocks = additional_bytes / block_size;
            if(additional_bytes % block_size)
                additional_blocks++;

            // add more blocks if we need to
            int block_chunk_size = 4096;
            uintptr_t *blocklist = (uintptr_t *)malloc(min(block_chunk_size, additional_blocks) * sizeof(uintptr_t));
            assert(blocklist);

            if(additional_blocks)
                truncate(m_offset + count);

            while(additional_blocks)
            {
                int block_offset = cached_size / block_size;
                if(cached_size % block_size > 0) block_offset++;

                int new_blocks = min(additional_blocks, block_chunk_size);

                int blockret = fosGetBlocklist(InodeOnly(m_inode), block_offset, new_blocks, blocklist);

                blocks(block_offset, new_blocks, blocklist);

                // zero out incoming blocks
                for(int i = 0; i < new_blocks; i++)
                    fosBufferCacheClear(blocklist[i], block_size);

                additional_blocks -= new_blocks;
                cached_size += new_blocks * block_size;
            }

            free(blocklist);
        }

        //FIXME: do some check here to make sure everything worked out?
        if(m_offset + count > m_size)
            m_size = m_offset + count;

        size_t offset = m_offset;

        while ((size_t) written < count) {
            size_t block_offset = offset & (block_size - 1);
            size_t block_nr = offset / block_size;
            size_t n = min((uint64_t)(block_size - block_offset), count - (size_t) written);
            assert(block_nr < m_blocks.size());
            const auto& block = m_blocks[block_nr];
            fosBufferCacheLog(BCL_ACCESS, inode().id, block);
            fosBufferCacheCopyIn(block + block_offset, buf, n);
            buf = (void*) ((uintptr_t) buf + n);
            offset += n;
            written += n;
        }
    }
    else
#endif
    while(written < count)
    {
        uint64_t write_size = std::min(count - written, (uint64_t) FS_CHUNK_SIZE);
        int ret = fosFdWrite(m_inode.server, m_fd, p + written, &write_size);

        if(ret != OK) return written ? written : ret;
        if(!write_size) break;

        written += write_size;
    }

    m_offset += written;
    return written;
}

int File::close()
{
    //Invalidate the cache before closing so that we flush any
    //modifications
    state(CACHE_INVALID);

    off64_t offset = -1;
    uint64_t filesize = UINT64_MAX;

#if ENABLE_BUFFER_CACHE
    if(m_state != CACHE_INVALID)
    {
        offset = m_offset;
        filesize = m_size;
        m_state = CACHE_INVALID;
    }
#endif

    return fosFdClose(m_inode.server, m_fd, offset, filesize);
}

int File::truncate(off_t len)
{
#if ENABLE_BUFFER_CACHE
    m_size = len;
    if(m_offset > m_size) m_offset = m_size;
#endif
    return fosTruncate(m_inode, "",len);
}

off64_t File::lseek(off64_t offset_, int whence)
{
#if ENABLE_BUFFER_CACHE
    if(m_state != CACHE_INVALID)
    {
        switch(whence)
        {
            case SEEK_SET:
                break;
            case SEEK_CUR:
                offset_ += m_offset;
                break;
            case SEEK_END:
                offset_ = m_size + offset_;
                break;
#ifdef SEEK_DATA
            case SEEK_DATA: // hare doesn't have holes...? always return data
                offset_ = m_offset;
                break;
            case SEEK_HOLE:
                offset_ = m_size; // implicit hole at end
                break;
#endif
            default:
                assert(false);
        }

        m_offset = offset_;
        return m_offset;
    }
    else
#endif
        return fosFdLseek(m_inode.server, m_fd, offset_, whence);
}

void File::size(size_t _size)
{
    m_size = _size;
#if ENABLE_BUFFER_CACHE
    int block_size = BufferCache::block_size;
    m_blocks.reserve(m_size / block_size);
#endif
}

void File::blocks(uint64_t offset, uint64_t count, uintptr_t *blocks)
{
    int block_chunk_size = 4096;
    assert(count <= block_chunk_size);
    for(int i = 0; i < count; i++)
    {
        if(i + offset < m_blocks.size())
            m_blocks[i + offset] = blocks[i];
        else
            m_blocks.push_back(blocks[i]);
    }
}

void File::cache_blocks()
{
#if ENABLE_BUFFER_CACHE
    int blocks_read = 0;
    int block_size = BufferCache::block_size;
    int block_chunk_size = 4096;
    int count = m_size / block_size;
    if(m_size % block_size) count++;

    uintptr_t *blocklist = (uintptr_t *)malloc(block_chunk_size * sizeof(uintptr_t));
    while(blocks_read < count)
    {
        int bc = min(block_chunk_size, count - blocks_read);
        int blockret = fosGetBlocklist(InodeOnly(m_inode), blocks_read, bc, blocklist);
        blocks(blocks_read, bc, blocklist);

        blocks_read += bc;
    }
    free(blocklist);
#endif
}

void File::state(int _state)
{
    if(_state == CACHE_INVALID && m_state == CACHE_EXCLUSIVE)
        flush();

    m_state = _state;
}

void File::flush()
{
#if ENABLE_BUFFER_CACHE
    fosFdFlush(m_inode.server, m_fd, m_size, m_offset);
#endif
}

}}}
