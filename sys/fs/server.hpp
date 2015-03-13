#pragma once

#include <cerrno>
#include <ctime>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <sys/types.h>
#include <utime.h>

#include <messaging/channel.h>

#include <sys/fs/fs.h>

// used for stats gathering
#include <sys/fs/interface.h>

#include "file.hpp"

namespace fos { namespace fs {

    typedef unordered_map<file_id_t, VFile*> InodeMap;

    class OpCounter;

    class FileDescriptor
    {
    public:
        FileDescriptor(file_id_t id_)
            : m_count(1), m_offset(0), m_id(id_), m_fd(rand()) {}
        FileDescriptor(const FileDescriptor&) = delete;
        FileDescriptor& operator=(const FileDescriptor&) = delete;

        int count() { return m_count; }
        void add() { m_count++; }
        void remove() { m_count--; }

        uint64_t offset() { return m_offset; }
        void offset(uint64_t m_) { m_offset = m_; }

        file_id_t id() { return m_id; }
        fd_t fd() { return m_fd; }

        const std::string & name() { return m_name; }
        void name(const std::string & n_) { m_name = n_; }

    private:
        uint64_t m_count;
        uint64_t m_offset;
        std::string m_name;
        file_id_t m_id;
        fd_t m_fd;
    };

    typedef unordered_map<fd_t, FileDescriptor*> FdMap;

    typedef unordered_set<VFileLoc *> AppInfo;
    typedef unordered_map<uintptr_t, AppInfo> AppMap;

    class FilesystemServer
    {
    public:
        FilesystemServer(Endpoint *ep, int id);
        ~FilesystemServer();

        FilesystemServer(const FilesystemServer&) = delete;
        FilesystemServer& operator=(const FilesystemServer&) = delete;

        static FilesystemServer & fs() { return *g_fs; }
        static unsigned int id;
        static void setId(int id_) { FilesystemServer::id = id_; }
        static bool started() { return g_fs != 0; }

        // used to manage application caching of dir lookups
        void appClose(uintptr_t appid);
        void appAddCache(uintptr_t appid, VFileLoc *);
        void appAddCache(uintptr_t appid, Inode parent, const char *path);
        void appRemoveCache(uintptr_t appid, VFileLoc *);

        // inode creation, mapping, etc...
        // these are combined to do open(,O_CREAT) and mkdir()
        int create(Inode parent, const char *path, int flags, mode_t mode, InodeType &exist);
        int destroy(file_id_t);
        int addmap(uintptr_t appid, Inode parent, const char *path, InodeType child, bool overwrite, InodeType *exist);
        int removemap(uintptr_t appid, file_id_t parent_inode, const char *path);

        // Operations
        // operations relating to shared fds...
        int pipe(fd_t& read_fd, fd_t& write_fd, Inode &inode);
        int open(file_id_t, int flags, fd_t& fd, int& type, size_t& size, int& state);
        int dup(fd_t, Inode& inode, int& type, uint64_t& size);
        int read(fd_t, char *buf, uint64_t *count);
        int write(fd_t, const char *buf, uint64_t *count);
        int lseek(fd_t, uint64_t &offset, int whence);
        int close(fd_t, off64_t offset, uint64_t size);

        // general fs operations
        int read(file_id_t, uint64_t offset, char *buf, uint64_t *count);
        int write(file_id_t, uint64_t offset, const char *buf, uint64_t *count);
        int mkdir(file_id_t, int parent_server, char *, mode_t mode, file_id_t *id);
        int fstatat(file_id_t, struct FauxStat64 *buf);
        int direntry(uintptr_t appid, file_id_t, int offset, uint64_t *size, char *buf, bool *more);
        int rmdir(file_id_t, int phase);
        int unlink(file_id_t);
        int readlink(file_id_t, char *result, uint64_t *size);
        int truncate(file_id_t, uint64_t len);
        int chmod(file_id_t, mode_t mode);
        int chown(file_id_t, uid_t, gid_t);
        int resolve(uintptr_t appid, file_id_t, char*, InodeType *exist);
        int utimens(file_id_t, struct timespec*, struct timespec*, int);
        int symlink(file_id_t, char *, const char*);

        // returns a list of blocks associated with a file
        int blocks(file_id_t, uintptr_t *out, uint64_t off, uint64_t cnt);

        // flush a file-descriptor that was modified directly by the application
        int flush(fd_t, uint64_t, off64_t);

        InodeMap::iterator find(file_id_t inode) { return m_files.find(inode); }
        InodeMap::iterator end() { return m_files.end(); }

        void opStart(const OpCounter &c);
        void opEnd(const OpCounter &c);

        void idle();

    private:
        void update_stat_file(Inode &file);
        void clear_stats();

        int m_id;
        bool m_master;

        Dir m_root;
        Dir m_orphans;
        InodeMap m_files;
        InodeMap m_pipes;
        FdMap m_fds;

        static FilesystemServer * g_fs;

        std::shared_ptr<BufferCache> m_buffer_cache;

        AppMap m_apps;

        //stats
        uint64_t m_op_count[NUM_FS_TYPES];
        uint64_t m_op_times[NUM_FS_TYPES];
    };

    class OpCounter
    {
    public:
        OpCounter(int id);
        ~OpCounter();

        int id() const { return m_id; }
        uint64_t start() const { return m_start; }
    private:
        int m_id;
        uint64_t m_start;
    };
}}

