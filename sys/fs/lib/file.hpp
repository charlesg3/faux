#pragma once

#include <vector>

#include <messaging/dispatch.h>
#include "vfile.hpp"

typedef uint64_t file_id_t;

namespace fos { namespace fs { namespace app {
    class File : public VFile
    {
    public:
        File(InodeType inode_, fd_t fd_);
        File(const File&) = delete;
        File& operator=(const File&) = delete;
        ~File();

        uint64_t read(void *buf, uint64_t count);
        uint64_t write(const void *buf, uint64_t count);
        int poll();
        int truncate(off_t size);
        off64_t lseek(off64_t offset, int whence);
        int close();

        off64_t offset() { return m_offset; }
        void offset(off64_t _offset) { m_offset = _offset; }

        void cache_blocks();
        void blocks(uint64_t offset, uint64_t count, uintptr_t *blocks);
        void size(size_t _size);
        size_t size() { return m_size; }

        void state(int _state);
        int state() { return m_state; }

        void flush();

    private:
        size_t m_size;
        int m_state;
        uint64_t m_offset;
        std::vector<uintptr_t> m_blocks;
    };
}}}
