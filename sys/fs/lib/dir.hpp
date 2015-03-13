#pragma once
#include <list>

#include <messaging/dispatch.h>
#include <sys/fs/fs.h>
#include "vfile.hpp"

namespace fos { namespace fs { namespace app {
    class Dir : public VFile
    {
    typedef std::list<FauxDirent *> DentList;
    public:
        Dir(InodeType inode_, fd_t fd_, uint64_t offset_ = 0);
        ~Dir();

        int getdents(struct linux_dirent *dirp, uint64_t count);
        int getdents64(struct linux_dirent64 *dirp, uint64_t count);

        int adddents(void *data, uint64_t size, int server);

        uint64_t offset() { return m_offset; }
        int close();

    protected:
        uint64_t m_offset;
        uint64_t m_current_server;
        DentList m_dents;

    private:
        void cache_dents();
    };
}}}
