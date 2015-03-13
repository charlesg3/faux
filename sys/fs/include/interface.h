#pragma once
#pragma GCC diagnostic ignored "-pedantic"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <sys/fs/fs.h>

enum MessageTypes
{
    CREATE = 0,
    DESTROY,
    ADDMAP,
    REMOVEMAP,
    PIPE,
    OPEN,
    READ,
    WRITE,
    LSEEK,
    CLOSE,
    DUP,
    FSTAT,
    DIRENT,
    UNLINK,
    READLINK,
    RMDIR,
    TRUNC,
    CHMOD,
    CHOWN,
    RESOLVE,
    UTIMENS,
    SYMLINK,
    BLOCKS,
    FLUSH,
    NUM_FS_TYPES
};

// RPC routines (synchronous)
struct ReadRequest
{
    fd_t fd;
    uint64_t count;
} __attribute__ ((packed));

struct ReadReply
{
    int retval;
    uint64_t count;
    char data[0];
} __attribute__ ((packed));

struct WriteRequest
{
    fd_t fd;
    uint64_t count;
    char data[0];
} __attribute__ ((packed));

struct LseekRequest
{
    fd_t fd;
    uint64_t offset;
    int whence;
    char data[0];
} __attribute__ ((packed));

struct LseekReply
{
    int retval;
    uint64_t offset;
} __attribute__ ((packed));

struct WriteReply
{
    int retval;
    uint64_t count;
} __attribute__ ((packed));

struct CloseRequest
{
    fd_t fd;
    off64_t offset;
    uint64_t size;
} __attribute__ ((packed));

struct AddMapRequest
{
    Inode parent;
    InodeType child;
    bool overwrite;
    bool update;
    char path[0];
} __attribute__ ((packed));

struct UpdateMapRequest
{
    Inode parent;
    InodeType child;
    bool overwrite;
    bool update;
    InodeType old_parent;
    char path[0];
} __attribute__ ((packed));

struct TruncRequest
{
    file_id_t inode;
    uint64_t len;
} __attribute__ ((packed));

struct FauxStat64
{
    uint64_t st_dev;           // Device
    uint64_t st_ino;           // File serial number
    uint64_t st_nlink;         // Link count
    uint64_t st_mode;          // File mode
    uint64_t st_uid;           // User ID of the file's owner
    uint64_t st_gid;           // Group ID of the file's group
    uint64_t st_rdev;          // Device number, if device
    uint64_t st_size;          // Size of file, in bytes
    uint64_t st_blksize;       // Optimal block size for I/O
    uint64_t st_blocks;        // Nr. 512-byte blocks allocated
    struct timespec _st_atime; // Time of last access
    struct timespec _st_mtime; // Time of last modification
    struct timespec _st_ctime; // Time of last status change
} __attribute__ ((packed));

struct StatReply
{
    int retval;
    struct FauxStat64 buf;
} __attribute__ ((packed));

struct DirEntRequest
{
    file_id_t inode;
    uint64_t offset;
    uint64_t size;
} __attribute__ ((packed));

struct FauxDirent
{
    ino64_t d_ino;
    off64_t d_off;
    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[256];		/* We must not include limits.h! */
} __attribute__ ((packed));

struct DirEntReply
{
    int retval;
    bool more;
    uint64_t size;
    char buf[0];
} __attribute__ ((packed));

struct ChmodRequest
{
    file_id_t inode;
    mode_t mode;
} __attribute__ ((packed));

struct RmdirRequest
{
    file_id_t id;
    int phase;
} __attribute__ ((packed));

struct RmdirAndMapRequest
{
    file_id_t id;
    int phase;
    InodeType parent;
    char path[0];
} __attribute__ ((packed));

struct ChownRequest
{
    file_id_t inode;
    uid_t uid;
    uid_t gid;
} __attribute__ ((packed));

struct ResolveReply
{
    InodeType child;
    int retval;
    uint64_t size;
    char path[0];
} __attribute__ ((packed));

struct PipeRequest {
    int flags;
} __attribute__ ((packed));

struct PipeReply
{
    int retval;
    fd_t read_fd;
    fd_t write_fd;
    Inode inode;
} __attribute__ ((packed));

struct CreateRequest {
    InodeType parent;
    int flags;
    mode_t mode;
    char path[0];
} __attribute__ ((packed));

struct OpenRequest {
    int flags;
    file_id_t id;
} __attribute__ ((packed));

struct CreateOpenRequest {
    int flags;
    Inode parent;
    mode_t mode;
    char path[0];
} __attribute__ ((packed));

struct OpenReply {
    int retval;
    fd_t fd;
    int type;
    uint64_t size;
    int state;
    InodeType exist;
} __attribute__ ((packed));

struct UnlinkRequest {
    file_id_t id;
    file_id_t parent_id;
    char path[0];
} __attribute__ ((packed));

struct DupRequest {
    fd_t fd;
} __attribute__ ((packed));

struct DupReply {
    int retval;
    Inode inode;
    int type;
    uint64_t size;
} __attribute__ ((packed));

struct UTimeNSRequest {
    file_id_t inode;
    struct timespec atime;
    struct timespec mtime;
    int flags;
} __attribute__ ((packed));

struct SymlinkRequest {
    file_id_t parent_id;
    uint64_t separator;
    char paths[0];
} __attribute__ ((packed));

struct BlockListRequest {
    file_id_t inode;
    uint64_t offset;
    uint64_t count;
};

struct BlockListReply {
    int retval;
    uint64_t count;
    bool more;
    uintptr_t blocks[0];
} __attribute__ ((packed));

struct FlushRequest {
    fd_t fd;
    uint64_t size;
    off64_t offset;
} __attribute__ ((packed));

// Generic RPCs

struct IdRequest {
    file_id_t id;
} __attribute__ ((packed));

struct IdPathRequest
{
    file_id_t id;
    char path[0];
} __attribute__ ((packed));

struct StatusReply
{
    int retval;
} __attribute__ ((packed));

struct StatusFdReply
{
    int retval;
    fd_t fd;
} __attribute__ ((packed));

struct StatusFdInodeReply
{
    int retval;
    fd_t fd;
    Inode inode;
} __attribute__ ((packed));

struct StatusInodeReply
{
    int retval;
    Inode inode;
} __attribute__ ((packed));

struct StatusInodeTypeReply
{
    int retval;
    InodeType inode;
} __attribute__ ((packed));

struct InodeNotify
{
    Inode inode;
} __attribute__ ((packed));

struct PathReply
{
    int retval;
    uint64_t size;
    char path[0];
} __attribute__ ((packed));

