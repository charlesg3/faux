#pragma once
#include <sched.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <stdbool.h>

#include <sys/fs/fs.h>
#include <sys/fs/interface.h>
#include <messaging/channel_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FD_COUNT CONFIG_FD_COUNT

// Library functions

void fsReset(void);
void fsCloneFds(void);
void fsCloseFds(void);
void fsFlattenFdInternal(int);
void fsFlattenFds(void);

struct linux_dirent {
    uint32_t        d_ino;
    int32_t         d_off;
    unsigned short  d_reclen;
    char            d_name[MAX_FNAME_LEN];
};

struct linux_dirent64 {
    uint64_t        d_ino;
    int64_t         d_off;
    unsigned short  d_reclen;
    unsigned char   d_type;
    char            d_name[MAX_FNAME_LEN];
};

Channel *establish_fs_chan(int i);

// vfile functions
InodeType fosFileIno(void *vp);
fd_t fosFileFd(void *vp);
bool fosIsDir(void *vp);
bool fosIsFile(void *vp);
bool fosIsPipe(void *vp);
void fosPipeAlloc(void** vp, InodeType inode, fd_t fd);
void fosDirAlloc(void** vp, InodeType inode, fd_t fd);
void fosFileAlloc(void** vp, InodeType inode, fd_t fd);
int fosFileTruncate(void *vp, off64_t length);

// dir functions
int fosGetDents(void *vp, struct linux_dirent *dirp, uint64_t count);
int fosGetDents64(void *vp, struct linux_dirent64 *dirp, uint64_t count);
int fosFreeDir(void *vp);

// fd functions
int fosFdRead(int server, fd_t fd, char *data, uint64_t *count);
int fosFdWrite(int server, fd_t fd, char *data, uint64_t *count);
int fosFdClose(int server, fd_t fd, off64_t offset, uint64_t size);
int fosFdFlush(int server, fd_t fd, uint64_t size, off64_t offset);
int fosFileDup(int server, fd_t fd);
off64_t fosFdLseek(int server, fd_t fd, off64_t offset, int whence);
int fosFileDescriptorCopy(void **vp, fd_t fd, InodeType inode);

// server marshalled functions
// functions that require a child
int fosCreate(InodeType, const char *path, int flags, mode_t mode, Inode *exist);
int fosDestroy(file_id_t inode);
int fosAddMap(int server, InodeType parent, const char *path, InodeType child, bool overwrite, InodeType *exist);
int fosRemoveMap(int server, InodeType parent, const char *path);
int fosUpdateMap(int server, InodeType parent, const char *path, InodeType child, bool overwrite, InodeType *exist, InodeType old_parent, const char *old_path);
int fosFileOpen(void **vp, InodeType parent, const char *path, int flags, mode_t mode);
int fosMkdir(InodeType, const char *, mode_t mode);
int fosChmod(InodeType, const char *, mode_t mode);
int fosChown(InodeType, const char *, uid_t uid, gid_t gid);
int fosTruncate(InodeType, const char *, off64_t length);
int fosFStatAt(InodeType, const char *, struct stat *buf, bool deref);
int fosFStatAt64(InodeType, const char *, struct stat64 *buf, bool deref);
int fosDirent(InodeType, off64_t offset, uint64_t *size, void *buf, bool *more);
int fosAccess(InodeType, const char *, int mode);
int fosUnlink(InodeType, const char *);
int fosFileLink(const char *oldpath, const char *newpath);
int fosRename(InodeType, const char *oldpath, const char *newpath);
ssize_t fosReadlink(InodeType, const char *, char *buf, size_t bufsiz);
int fosRmdir(InodeType, const char *);
int fosUTime(InodeType, const char*, const struct utimbuf*);
int fosUTimeNS(InodeType, const char*, const struct timespec*, int);
int fosFileSymlink(InodeType, const char*, const char*);
int fosPipeOpen(void **vp_read, void **vp_write);

void _fosClearCache(InodeType parent, const char *path, int pathlen); // used internally when a cache entry fails
int _fosResolve(InodeType parent, const char *path, int pathlen, InodeType *result); // function which also takes the pathlen used internally
int fosResolveFull(InodeType inode, char *buf, size_t *size);
int fosLookup(InodeType inode, const char *path, InodeType *result, bool *cached);
int fosLookupParent(InodeType parent, const char **path, InodeType *result, bool *cached);

// readlink function that just takes an server + inode since some functiongs
// (e.g. fstat / open) will encounter symlinks and need to handle them
ssize_t fosDoReadlink(InodeType node, char *buf, size_t bufsiz);

// miscellanious function for separating virtual fs from underlying fs
bool fosPathExcluded(const char*path);

// closes hare fd but keeps linux fd open (used on CLOEXEC)
int __close_int(int internal_fd);

// Buffer Cache Management
void fosLoadBufferCache();
void fosBufferCacheCopyOut(void *dest, uint64_t offset, uint64_t count);
void fosBufferCacheCopyIn(uint64_t offset, const void *src, uint64_t count);
void fosBufferCacheClear(uint64_t offset, uint64_t count);

int fosGetBlocklist(Inode inode, uint64_t offset, uint64_t count, uintptr_t *list);

// Misc
void fosCleanup();
void fosShowLocalOps();
void fosNotifications(void *, size_t);
void fosDirCacheClear();

#ifdef __cplusplus
}
#endif
