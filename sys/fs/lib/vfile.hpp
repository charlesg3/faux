#pragma once

#include <sys/fs/fs.h>

namespace fos { namespace fs { namespace app {
class VFile
{
public:
    VFile(InodeType inode_, fd_t fd_)
        : m_inode(inode_), m_fd(fd_) {}

    bool isDir() { return m_inode.type == TYPE_DIR || m_inode.type == TYPE_DIR_SHARED; }
    bool isFile() { return m_inode.type == TYPE_FILE; }
    bool isPipe() { return m_inode.type == TYPE_PIPE; }
    bool isShared() { return m_inode.type == TYPE_DIR_SHARED; }

    virtual int close() = 0;

    InodeType inode() { return m_inode; }
    fd_t fd() { return m_fd; }

protected:
    const InodeType m_inode;
    const fd_t m_fd;
};
}}}
