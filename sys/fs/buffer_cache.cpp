#include <cassert>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <iostream>
#include <iterator>
#include <list>
#include <memory>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using namespace std;

#include "buffer_cache.hpp"

using namespace fos;
using namespace fs;

BufferCache::BufferCache(int id)
: m_id(id)
, m_blocks_free(memory_size / (block_size * CONFIG_FS_SERVER_COUNT))
{
    int flags = O_RDWR | O_CREAT | O_TRUNC;
    if(m_id == 0) flags |= O_EXCL;

    int fd = shm_open(memory_path, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    assert(fd >= 0);
    int retval __attribute__((unused)) = ftruncate(fd, memory_size);
    assert(retval == 0);
    void* memory = mmap(nullptr, memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(memory != MAP_FAILED);
    close(fd);
    m_memory = (uintptr_t) memory;

    // now distribute the blocks amongst the servers
    int server_count = CONFIG_FS_SERVER_COUNT;
    int partition_size = memory_size / server_count;

    uintptr_t region_start = (id * partition_size);
    uintptr_t region_end = ((id + 1) * partition_size);

    for (uintptr_t offset = region_start; offset < region_end; offset += block_size)
        m_blocks.push_back(m_memory + offset);
}

BufferCache::~BufferCache()
{
    munmap((void*) m_memory, memory_size);
    if(m_id == 0)
        shm_unlink(memory_path);
}

vector<uintptr_t> BufferCache::get(size_t blocks_nr)
{
    if (m_blocks_free < blocks_nr)
        return vector<uintptr_t> {};
    auto first = begin(m_blocks);
    auto last = begin(m_blocks);
    advance(last, blocks_nr);
    vector<uintptr_t> blocks {first, last};
    m_blocks.erase(first, last);
    m_blocks_free -= blocks_nr;
    return blocks;
}

void BufferCache::put(list<uintptr_t>&& blocks)
{
    m_blocks_free += blocks.size();
    m_blocks.splice(begin(m_blocks), blocks);
}

