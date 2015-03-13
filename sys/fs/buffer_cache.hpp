#ifndef _BUFFER_CACHE_HPP
# define _BUFFER_CACHE_HPP

# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include <config/config.h>

using namespace std;

namespace fos { namespace fs {

    class BufferCache {
        public:
            static constexpr auto memory_path = "/faux-shm-42";
            static constexpr size_t memory_size = CONFIG_BUFFER_CACHE_SIZE * 1024UL * 1024UL;
            static constexpr size_t block_size = CONFIG_BUFFER_CACHE_BLOCKSIZE;
        public:
            BufferCache(int id);
            BufferCache(const BufferCache&) = delete;
            BufferCache& operator=(const BufferCache&) = delete;
            ~BufferCache();

            vector<uintptr_t> get(size_t blocks_nr);
            void put(list<uintptr_t>&& blocks);

            uint64_t free() { return m_blocks_free * block_size; }
            uint64_t used() { return (memory_size / CONFIG_FS_SERVER_COUNT) - free(); }
            uint64_t total() { return memory_size / CONFIG_FS_SERVER_COUNT; }
            uint64_t block_count() { return m_blocks.size(); }
            uint64_t blocks_free() { return m_blocks_free; }
            uintptr_t start() { return m_memory; }

        private:
            int m_id;
            uintptr_t m_memory;
            list<uintptr_t> m_blocks;
            size_t m_blocks_free;
    };

}}

#endif

