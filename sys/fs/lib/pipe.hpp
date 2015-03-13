#pragma once

#include <messaging/dispatch.h>
#include "vfile.hpp"

namespace fos { namespace fs { namespace app {
    class Pipe : public VFile
    {
    public:
        Pipe(InodeType inode_, fd_t fd_read);
        Pipe(const Pipe&) = delete;
        Pipe& operator=(const Pipe&) = delete;
        ~Pipe();

        uint64_t read(void *buf, uint64_t count);
        uint64_t write(const void *buf, uint64_t count);
        int close();

    private:
        bool m_should_close;
        bool m_have_closed;
    };
}}}
