#pragma once

#include <ctime>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>
#include <list>
#include <vector>

using namespace std;

#include <sys/fs/fs.h>
#include "buffer_cache.hpp"

namespace fos { namespace fs {

    class VFile;
    class VFileLoc;

    typedef unordered_map<string, VFileLoc*> ChildMap;

    class VFileLoc
    {
    public:
        VFileLoc(InodeType _inode)
            : inode(_inode) {}
        VFileLoc(const VFileLoc&) = delete;
        VFileLoc& operator=(const VFileLoc&) = delete;

        bool isDir() const { return inode.type == TYPE_DIR || inode.type == TYPE_DIR_SHARED; }
        bool isFile() const { return inode.type == TYPE_FILE; }
        bool isSymlink() const { return inode.type == TYPE_SYMLINK; }
        bool isPipe() const { return inode.type == TYPE_PIPE; }
        bool isShared() const { return inode.type == TYPE_DIR_SHARED; }


        InodeType inode;
    };

    class VFile
    {
    public:
        VFile(FileType type, mode_t mode = 0777);
        VFile(const VFile&) = delete;
        VFile& operator=(const VFile&) = delete;
        virtual ~VFile();

        virtual file_id_t id() const { return m_id; }
        virtual void id(file_id_t id_) { m_id = id_; }

        FileType type() const { return m_type; }
        bool isDir() const { return m_type == TYPE_DIR || m_type == TYPE_DIR_SHARED; }
        bool isFile() const { return m_type == TYPE_FILE; }
        bool isSymlink() const { return m_type == TYPE_SYMLINK; }
        bool isShared() const { return m_type == TYPE_DIR_SHARED; }

        mode_t mode() const { return m_mode; }
        void mode(mode_t mode_) { m_mode = mode_; }

        struct timespec atime() const { return m_atime; }
        void atime(struct timespec& t) { m_atime = t; }
        struct timespec mtime() const { return m_mtime; }
        void mtime(struct timespec& t) { m_mtime = t; }
        struct timespec ctime() const { return m_ctime; }
        void ctime(struct timespec& t) { m_ctime = t; }

        uid_t uid() const { return m_uid; }
        void uid(uid_t _u) { if(_u != UINT32_MAX) m_uid = _u; }
        gid_t gid() const { return m_gid; }
        void gid(gid_t _g) { if(_g != UINT32_MAX) m_gid = _g; }

        virtual size_t size() = 0;

    protected:
        file_id_t m_id;

    private:
        FileType m_type;
        mode_t m_mode;
        struct timespec m_atime;
        struct timespec m_mtime;
        struct timespec m_ctime;
        uid_t m_uid;
        gid_t m_gid;
    };

    class File : public VFile
    {
    public:
        File(shared_ptr<BufferCache> buffer_cache, mode_t mode_ = 0644);
        File(const File&) = delete;
        File& operator=(const File&) = delete;
        ~File();

        ssize_t read(void* buf, size_t count, size_t offset);
        ssize_t write(const void* buf, size_t count, size_t offset);
        int truncate(size_t size);

        size_t size() { return m_size; }
        void size(size_t _size) { m_size = _size; }

        size_t capacity();

        int blocks(uintptr_t *blocklist, int offset, int count);

    private:
        int shrink(size_t new_size);
        int grow(size_t new_size);
        void zero(size_t new_size);

    private:
        shared_ptr<BufferCache> m_buffer_cache;
        vector<uintptr_t> m_memory_blocks;
        vector<uintptr_t> m_orphan_blocks;
        size_t m_size;
    };

    class Dir : public VFile
    {
    public:
        Dir(bool vdir, mode_t mode_ = 0777);
        Dir(const Dir&) = delete;
        Dir& operator=(const Dir&) = delete;
        ~Dir();

        bool isVirtual() { return m_vdir; }
        size_t size() { return 4096; }

        void lock(bool l_ = true) { m_locked = l_; }
        bool locked() { return m_locked; }

        void addChild(const std::string & name, InodeType inode);
        ChildMap& children() { return m_children; }
        ChildMap::iterator find(const string & needle) { return m_children.find(needle); }
        ChildMap::iterator find(file_id_t needle);
        ChildMap::iterator begin() { return m_children.begin(); }
        ChildMap::iterator end() { return m_children.end(); }
        void erase(ChildMap::iterator needle);
        void erase(const string & needle);

    private:
        bool m_vdir;
        bool m_locked;
        ChildMap m_children;
    };

    class Pipe : public VFile
    {
    public:
        Pipe(mode_t mode_ = 0777);
        Pipe(const Pipe&) = delete;
        Pipe& operator=(const Pipe&) = delete;
        ~Pipe();

        ssize_t read(void* buf, size_t count, size_t offset);
        ssize_t write(const void* buf, size_t count, size_t offset);
        size_t size() { return 0; }

        void closeRead();
        void closeWrite();

        fd_t readFd() { return m_read_fd; }
        void readFd(fd_t read_) { m_read_fd = read_; }
        fd_t writeFd() { return m_write_fd; }
        void writeFd(fd_t write_) { m_write_fd = write_; }

    private:
        size_t m_size;
        fd_t m_read_fd;
        fd_t m_write_fd;

        int m_waiters;
        ConditionVariable m_cv;
    };

    class Symlink : public VFile
    {
    public:
        Symlink(const string & dest, mode_t mode = 0777);
        size_t size() { return m_dest.size(); }
        const string& dest() const { return m_dest; }

    private:
        const string m_dest;
    };
}}

