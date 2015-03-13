#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef VDSOWRAP
#include <vdsowrap/vdsowrap.h>
#else
#endif

#ifdef VDSOWRAP
// Wrappers to underlying sys calls
#define real_open(path,flags,mode) vdso_invoke3(SYS_open, path, flags, mode)
#define real_read(fd,buf,count) vdso_invoke3(SYS_read, fd, buf, count)
#define real_write(fd,buf,count) vdso_invoke3(SYS_write, fd, buf, count)
#define real_close(fd) vdso_invoke1(SYS_close, fd)
#define real_stat(path,buf) vdso_invoke2(SYS_stat, path, buf)
#define real_stat64(path,buf) vdso_invoke2(SYS_stat64, path, buf)
#define real_lstat(path,buf) vdso_invoke2(SYS_lstat, path, buf)
#define real_lstat64(path,buf) vdso_invoke2(SYS_lstat64, path, buf)
#define real_lseek64(fd,offset,whence) vdso_invoke3(SYS_lseek, fd, offset, whence)
#define real__llseek(fd,offset_hi,offset_lo,result,whence) vdso_invoke5(SYS__llseek, fd, offset_hi, offset_lo, result, whence)
#define real_fstat(fd,buf) vdso_invoke2(SYS_fstat, fd, buf)
#define real_fstat64(fd,buf) vdso_invoke2(SYS_fstat64, fd, buf)
#define real_fstatat64(dirfd,path,buf,flags) vdso_invoke4(SYS_fstatat64, dirfd, path, buf, flags)
#define real_fchdir(fd) vdso_invoke1(SYS_fchdir, fd)
#define real_chdir(path) vdso_invoke1(SYS_chdir, path)
#define real_dup(a) vdso_invoke1(SYS_dup, a)
#define real_dup2(a,b) vdso_invoke2(SYS_dup2, a, b)
#define real_dup3(oldfd, newfd, flags) vdso_invoke3(SYS_dup3, oldfd, newfd, flags)
#define real_execve(f,a,e) vdso_invoke3(SYS_execve, f, a, e)
#define real_fcntl(fd,cmd,arg) vdso_invoke3(SYS_fcntl, fd, cmd, arg)
#define real_openat(fd,path,flags,mode) vdso_invoke4(SYS_openat, fd, path, flags, mode)
#define real_access(path,mode) vdso_invoke2(SYS_access, path, mode)
#define real_readlink(path,buf,size) vdso_invoke3(SYS_readlink, path, buf, size)
#define real_unlink(path) vdso_invoke1(SYS_unlink, path)
#define real_unlinkat(dirfd, path, flags) vdso_invoke3(SYS_unlinkat, dirfd, path, flags)
#define real_clone(clone_flags, newsp, parent_tidptr, child_tidptr, tls_val) vdso_invoke5(SYS_clone, clone_flags, newsp, parent_tidptr, child_tidptr, tls_val)
#define real_getcwd(buf, size) vdso_invoke2(SYS_getcwd, buf, size)
#define real_fchmod(fd,mode) vdso_invoke2(SYS_fchmod, fd, mode)
#define real_chmod(path,mode) vdso_invoke2(SYS_chmod, path, mode)
#define real_getdents(fd, dirp, count) vdso_invoke3(SYS_getdents, fd, dirp, count)
#define real_getdents64(fd, dirp, count) vdso_invoke3(SYS_getdents64, fd, dirp, count)
#define real_mmap2(addr, length, prot, flags, fd, offset) vdso_invoke6(SYS_mmap2, addr, length, prot, flags, fd, offset)
#define real_munmap(addr, length) vdso_invoke2(SYS_munmap, addr, length)
#define real_faccessat(dirfd, pathname, mode, flags) vdso_invoke4(SYS_faccessat, dirfd, pathname, mode, flags)
#define real_utime(filename, times) vdso_invoke2(SYS_utime, filename, times)
#define real_utimensat(dd, pathname, times, flags) vdso_invoke4(SYS_utimensat, dd, pathname, times, flags)
#define real_mkdirat(dir_fd, path, mode) vdso_invoke3(SYS_mkdirat, dir_fd, path, mode)
#define real_truncate(path, len) vdso_invoke2(SYS_truncate, path, len)
#define real_ftruncate64(fd, len) vdso_invoke2(SYS_ftruncate64, fd, len)
#define real_fsync(fd) vdso_invoke1(SYS_fsync, fd)
#define real_fdatasync(fd) vdso_invoke1(SYS_fdatasync, fd)
#define real_rename(old,new) vdso_invoke2(SYS_rename, old, new)
#define real_fchmodat(dir_fd, path, mode) vdso_invoke3(SYS_fchmodat, dir_fd, path, mode)
#define real_chown(path, owner, group) vdso_invoke3(SYS_chown, path, owner, group)
#define real_lchown(path, owner, group) vdso_invoke3(SYS_lchown, path, owner, group)
#define real_fchown(fd, owner, group) vdso_invoke3(SYS_fchown, fd, owner, group)
#define real_fchownat(dir_fd, path, owner, group) vdso_invoke4(SYS_fchownat, dir_fd, path, owner, group)
#define real_fsetxattr(fd, name, val, size, flags) vdso_invoke5(SYS_fsetxattr, fd, name, val, size, flags)
#define real_exit_group(status) vdso_invoke1(SYS_exit_group, status)
#define real_pipe(pipefd) vdso_invoke1(SYS_pipe, pipefd)
#define real_pipe2(pipefd, flags) vdso_invoke2(SYS_pipe2, pipefd, flags)
#define real_socketcall(call, args) vdso_invoke2(SYS_socketcall, call, args)
#define real_ioctl(fd, req, arg) vdso_invoke3(SYS_ioctl, fd, req, arg)
#define real_select(nfds, readfds, writefds, execfds, timeout) vdso_invoke5(SYS_select, nfds, readfds, writefds, execfds, timeout)
#define real_sigaction(signum, act, oldact) vdso_invoke3(SYS_sigaction, signum, act, oldact)
#define real_rt_sigaction(signum, act, oldact, size) vdso_invoke4(SYS_rt_sigaction, signum, act, oldact, size)

// posix stuff
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>
#include <system/dlfun.h>
#include <assert.h>

#else
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <system/dlfun.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>

static inline int real_open(const char *path, int flags, mode_t mode)
{
    static int (*real_open_f)(const char *path, int flags, mode_t mode) = dlfun(open);
    assert(real_open_f);
    return real_open_f(path, flags, mode);
}

static inline int real_openat(int dir_fd, const char *path, int flags, mode_t mode)
{
    static int (*real_openat_f)(int dir_fd, const char *path, int flags, mode_t mode) = dlfun(openat);
    assert(real_openat_f);
    return real_openat_f(dir_fd, path, flags, mode);
}

static inline int real_access(const char *path, int mode)
{
    static int (*real_access_f)(const char *path, int mode) = dlfun(access);
    assert(real_access_f);
    return real_access_f(path, mode);
}

static inline int real_fchdir(int fd)
{
    static int (*real_fchdir_f)(int) = dlfun(fchdir);
    assert(real_fchdir_f);
    return real_fchdir_f(fd);
}

static inline int real_chdir(const char *path)
{
    static int (*real_chdir_f)(const char *) = dlfun(chdir);
    assert(real_chdir_f);
    return real_chdir_f(path);
}

static inline char *real_getcwd(char *buf, size_t size)
{
    static char *(*real_getcwd_f)(char *, size_t) = dlfun(getcwd);
    assert(real_getcwd_f);
    return real_getcwd_f(buf, size);
}

static inline int real_unlink(const char *path)
{
    // See if this should be filtered
    static int (*real_unlink_f)(const char *path) = dlfun(unlink);
    assert(real_unlink_f);
    return real_unlink_f(path);
}

static inline int real_unlinkat(const char *path)
{
    // See if this should be filtered
    static int (*real_unlinkat_f)(const char *path) = dlfun(unlinkat);
    assert(real_unlinkat_f);
    return real_unlinkat_f(path);
}

static inline int real_close(int fd)
{
    static int (*real_close_f)(int fd) = dlfun(close);
    assert(real_close_f);
    return real_close_f(fd);
}

static inline int real_dup2(int oldfd, int newfd)
{
    static int (*real_dup2_f)(int oldfd, int newfd) = dlfun(dup2);
    assert(real_dup2_f);
    return real_dup2_f(oldfd, newfd);
}

static inline int real_dup(int oldfd, int newfd)
{
    static int (*real_dup_f)(int oldfd, int newfd) = dlfun(dup);
    assert(real_dup_f);
    return real_dup_f(oldfd, newfd);
}

static inline int real_stat(const char *path, struct stat *buf)
{
    static int (*real___xstat_f)(int stat_ver, const char *path, struct stat *buf) = dlfun(__xstat);
    assert(real___xstat_f);
    return real___xstat_f(1, path, buf);
}

static inline int real_stat64(const char *path, struct stat64 *buf)
{
    static int (*real___xstat64_f)(int stat_ver, const char *path, struct stat64 *buf) = dlfun(__xstat64);
    assert(real___xstat64_f);
    return real___xstat64_f(1, path, buf);
}

static inline int real_lstat64(const char *path, struct stat64 *buf)
{
    static int (*real___lxstat64_f)(int stat_ver, const char *path, struct stat64 *buf) = dlfun(__lxstat64);
    assert(real___lxstat64_f);
    return real___lxstat64_f(1, path, buf);

}

static inline int real_fstat(int fd, struct stat *buf)
{
    static int (*real___fxstat_f)(int stat_ver, int fd, struct stat *buf) = dlfun(__fxstat);
    assert(real___fxstat_f);
    return real___fxstat_f(1, fd, buf);
}

static inline int real_fstat64(int fd, struct stat64 *buf)
{
    static int (*real___fxstat64_f)(int stat_ver, int fd, struct stat64 *buf) = dlfun(__fxstat64);
    assert(real___fxstat64_f);
    return real___fxstat64_f(1, fd, buf);
}

static inline int real_lstat(const char *path, struct stat *buf)
{
    static int (*real___lxstat_f)(int stat_ver, const char *path, struct stat *buf) = dlfun(__lxstat);
    assert(real___lxstat_f);
    return real___lxstat_f(1, path, buf);
}


static inline int real_execve(const char *filename, char *const argv[], char *const envp[])
{
    static int (*real_execve_f)(const char *filename, char *const argv[], char *const envp[]) = dlfun(execve);
    assert(real_execve_f);
    return real_execve_f(filename, argv, envp);
}

static inline ssize_t real_write(int fd, const void * buf, size_t count)
{
    static int (*real_write_f)(int fd, const void * buf, size_t count) = dlfun(write);
    assert(real_write_f);
    return real_write_f(fd, buf, count);
}

static inline off64_t real_lseek64(int fd, off64_t offset, int whence)
{
    static off64_t (*real_lseek64_f)(int fd, off64_t offset, int whence) = dlfun(lseek64);
    assert(real_lseek64_f);
    return real_lseek64_f(fd, offset, whence);
}

static inline int real_fcntl(int fd, int cmd, ...)
{
    static int (*real_fcntl_f)(int fd, int cmt, long arg) = dlfun(fcntl);
    assert(real_fcntl_f);

    va_list var_args;
    va_start(var_args, cmd);
    long arg = va_arg(var_args, long);

    return real_fcntl_f(fd, cmd, arg);
}

static inline ssize_t real_read(int fd, void *buf, size_t nbytes)
{
    static int (*real_read_f)(int fd, void * buf, size_t nbytes) = dlfun(read);
    assert(real_read_f);
    return real_read_f(fd, buf, nbytes);
}


static inline ssize_t real_readlink(const char *path, char *buf, size_t bufsiz)
{
    static int (*real_readlink_f)(const char *path, char *buf, size_t bufsiz) = dlfun(readlink);
    assert(real_readlink_f);
    return real_readlink_f(path, buf, bufsiz);
}

static inline int real_chmod(const char *path, mode_t mode)
{
    static int (*real_chmod_f)(const char *path, mode_t mode) = dlfun(chmod);
    assert(real_chmod_f);
    return real_chmod_f(path, mode);
}

static inline int real_fchmod(int fd, mode_t mode)
{
    static int (*real_fchmod_f)(int fd, mode_t mode) = dlfun(fchmod);
    assert(real_fchmod_f);
    return real_fchmod_f(fd, mode);
}


// I don't think this scheme works because libc clone() != sys_clone(), therefore
// we need to have some sys-clone to be compatible w/ 64-bit, or intercept libc
// clone() with a different protoype and functionality because the libc-clone
// calls the function while the syscall does not.
#if 0
static inline int real_clone(int (*fn)(void *), void *stack, int flags, void *arg)
{
    static int (*real_clone_f)(int (*fn)(void *), void *stack, int flags, void *arg) = dlfun(clone);
    assert(real_clone_f);
    return real_clone_f(fn, stack, flags, arg);
}
#endif

#endif

// posix stuff
static inline DIR *real_opendir(const char *name)
{
    static DIR *(*real_opendir_f)(const char *path) = dlfun(opendir);
    assert(real_opendir_f);
    return real_opendir_f(name);
}

static inline struct dirent *real_readdir(DIR *dirp)
{
    static struct dirent *(*real_readdir_f)(DIR *dirp) = dlfun(readdir);
    assert(real_readdir_f);
    return real_readdir_f(dirp);
}

static inline struct dirent64 *real_readdir64(DIR *dirp)
{
    static struct dirent64 *(*real_readdir64_f)(DIR *dirp) = dlfun(readdir64);
    assert(real_readdir64_f);
    return real_readdir64_f(dirp);
}

static inline int real_dirfd(DIR *dirp)
{
    static int (*real_dirfd_f)(DIR *dirp) = dlfun(dirfd);
    assert(real_dirfd_f);
    return real_dirfd_f(dirp);
}

static inline DIR *real_fdopendir(int fd)
{
    static DIR *(*real_fdopendir_f)(int fd) = dlfun(fdopendir);
    assert(real_fdopendir_f);
    return real_fdopendir_f(fd);
}

static inline int real_closedir(DIR *p)
{
    static int (*real_closedir_f)(DIR *p) = dlfun(closedir);
    assert(real_closedir_f);
    return real_closedir_f(p);
}

