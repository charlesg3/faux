#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <utime.h>
#include "vdsowrap.h"

struct linux_dirent;
struct linux_dirent64;

extern ssize_t __write(int fd, const void * buf, size_t count);
extern ssize_t __read(int fd, void *buf, size_t nbytes);
extern int __stat(const char *path, struct stat *buf);
extern int __stat64(const char *path, struct stat64 *buf);
extern int __lstat64(const char *path, struct stat64 *buf);
extern int __fstat(int fd, struct stat *buf);
extern int __fstatat64(int dir_fd, const char *path, struct stat64 *buf, int flags);
extern int __fstat64(int fd, struct stat64 *buf);
extern int __lstat(const char *path, struct stat *buf);
extern int __open(const char *path, int flags, mode_t mode);
extern int __openat(int dirfd, const char *path, int flags, mode_t mode);
extern int __faccessat(int dirfd, const char *path, int mode, int flags);
extern long __clone(unsigned long clone_flags, unsigned long newsp, int *parent_tidptr, int *child_tidptr, int tls_val);
extern long __getcwd(char *buf, unsigned long size);
extern long ___llseek(unsigned int fd, unsigned long offset_high, unsigned long offset_low, loff_t *result, unsigned int whence);
extern int __chmod(const char *path, mode_t mode);
extern int __fchmod(int fd, mode_t mode);
extern int __fchmodat(int dir_fd, const char *path, mode_t mode, int flags);
extern int __getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count);
extern int __getdents64(unsigned int fd, struct linux_dirent64 *dirp, unsigned int count);
extern int __execve(const char *filename, char *const argv[], char *const envp[]);
extern void *__mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int __munmap(void *addr, size_t length);
extern int __access(const char*, int);
extern int __creat(const char *path, mode_t);
extern int __faccessat(int, const char*, int, int);
extern int __symlink(const char*, const char*);
extern int __unlink(const char*);
extern int __unlinkat(int, const char*, int);
extern int __mkdir(const char *pathname, mode_t mode);
extern int __mkdirat(int dir_fd, const char *path, mode_t mode);
extern int __readlink(const char*, char*, size_t);
extern int __rename(const char*, const char*);
extern int __rmdir(const char*);
extern int __close(int);
extern int __pipe(int pipefd[2]);
extern int __pipe2(int pipefd[2], int);
extern int __utime(const char*, const struct utimbuf*);
extern int __utimensat(int dd, const char* pathname, const struct timespec times[2], int flags);
extern int __dup(int);
extern int __dup2(int, int);
extern int __dup3(int, int, int);
extern int __chdir(const char *path);
extern int __fchdir(int fd);
extern int __symlinkat(const char*, int, const char*);
extern int __fcntl(int fd, int cmd, ...);
extern int __truncate(const char *, off_t);
extern int __ftruncate(const char *, off_t);
extern int __ftruncate64(const char *, off_t);
extern int __fsync(int fd);
extern int __fdatasync(int fd);
extern int __chown(const char *path, uid_t owner, gid_t group);
extern int __lchown(const char *path, uid_t owner, gid_t group);
extern int __fchown(int fd, uid_t owner, gid_t group);
extern int __fchownat(int dir_fd, const char *path, uid_t owner, gid_t group);
extern int __fsetxattr(int fd, const char *name, const void *value, size_t size, int flags);
extern int __exit_group(int status);
extern int __socketcall(int, unsigned long *args);
extern int __ioctl(int fd, int request, ...);
extern int __select(int nfds, fd_set *, fd_set *, fd_set *, struct timeval *);
extern int __sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
extern int __rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact, int size);

enum { print_syscalls = 0 };

/*
 * Ref:
 * http://justanothergeek.chdir.org/2010/03/seccomp-as-sandboxing-solution.html
 */

static void
(*get_vdso_gate(void))()
{
    void (*old_handler)();

    __asm__ __volatile__ (
            "mov %%gs:0x10, %%edx\n"
            "mov %%edx, %0\n"
            : "=m" (old_handler) : : "edx");

    return old_handler;
}

static void
set_vdso_gate(void (*new_handler)())
{
    __asm__ __volatile__ (
            "mov %0, %%edx\n"
            "mov %%edx, %%gs:0x10\n"
            : : "r" (new_handler) : "edx");
}

static void
__attribute__((constructor))
fake_vdso_init(void)
{
    vdso_kernel_handler = get_vdso_gate();
    set_vdso_gate(vdso_entry);
}

long
vdso_entry_c(long num, long a0, long a1, long a2, long a3, long a4, long a5)
{
    if (print_syscalls) {
        static int entry_active;
        if (!entry_active) {
            entry_active = 1;
            printf("vdso_entry_c: %ld %#lx %#lx %#lx %#lx %#lx %#lx\n",
                    num, a0, a1, a2, a3, a4, a5);
            entry_active = 0;
        }
    }

    switch (num) {
    case SYS_open:       return __open((const char *) a0, (int) a1, (mode_t) a2);
    case SYS_read:       return __read((int) a0, (void *) a1, (size_t) a2);
    case SYS_write:      return __write((int) a0, (const void *) a1, (size_t) a2);
    case SYS_close:      return __close((int) a0);
    case SYS_openat:     return __openat((int) a0, (const char *) a1, (int) a2, (mode_t) a3);
    case SYS_access:     return __access((const char *) a0, (int) a1);
    case SYS_fchdir:     return __fchdir((int) a0);
    case SYS_chdir:      return __chdir((const char *) a0);
    case SYS_unlink:     return __unlink((const char *) a0);
    case SYS_unlinkat:   return __unlinkat((int) a0, (const char*) a1, (int) a2);
    case SYS_getdents:   return __getdents((int)a0, (void *)a1, (unsigned int)a2);
    case SYS_getdents64: return __getdents64((int)a0, (void *)a1, (unsigned int)a2);
    case SYS_dup:        return __dup((int) a0);
    case SYS_dup2:       return __dup2((int) a0, (int) a1);
    case SYS_dup3:       return __dup3((int) a0, (int) a1, (int) a2);
    case SYS_stat:       return __stat((const char *) a0, (struct stat *) a1);
    case SYS_stat64:     return __stat64((const char *) a0, (struct stat64 *) a1);
    case SYS_lstat:      return __lstat((const char *) a0, (struct stat *) a1);
    case SYS_lstat64:    return __lstat64((const char *) a0, (struct stat64 *) a1);
    case SYS_fstat:      return __fstat((int) a0, (struct stat *) a1);
    case SYS_fstat64:    return __fstat64((int) a0, (struct stat64 *) a1);
    case SYS_fstatat64:  return __fstatat64((int) a0, (const char *) a1, (struct stat64 *) a2, (int) a3);
    case SYS_execve:     return __execve((const char *) a0, (char **) a1, (char **) a2);
    case SYS_fcntl:      return __fcntl((int) a0, (int) a1, (long) a2);
    case SYS_fcntl64:    return __fcntl((int) a0, (int) a1, (long) a2);
    case SYS_readlink:   return __readlink((const char*) a0, (char*) a1, (size_t) a2);
    case SYS_mkdir:      return __mkdir((const char *) a0, (mode_t) a1);
    case SYS_mkdirat:    return __mkdirat((int)a0, (const char *) a1, (mode_t) a2);
    case SYS_getcwd:     return __getcwd((char *) a0, (unsigned long) a1);
    case SYS_clone:      return __clone((unsigned long) a0, (unsigned long) a1, (int *) a2, (int *) a3, (int) a4);
    case SYS__llseek:    return ___llseek((unsigned int) a0, (unsigned long) a1, (unsigned long) a2, (loff_t *) a3, (unsigned int) a4);
    case SYS_chmod:      return __chmod((const char *)a0, (mode_t)a1);
    case SYS_fchmod:     return __fchmod((int)a0, (mode_t)a1);
    case SYS_fchmodat:   return __fchmodat((int)a0, (const char *)a1, (mode_t)a2, (int)a3);
    case SYS_chown:      return __chown((const char *)a0, (uid_t)a1, (gid_t)a2);
    case SYS_lchown:     return __lchown((const char *)a0, (uid_t)a1, (gid_t)a2);
    case SYS_lchown32:   return __lchown((const char *)a0, (uid_t)a1, (gid_t)a2);
    case SYS_fchown:     return __fchown((int)a0, (uid_t)a1, (gid_t)a2);
    case SYS_fchown32:   return __fchown((int)a0, (uid_t)a1, (gid_t)a2);
    case SYS_fchownat:   return __fchownat((int)a0, (const char *)a1, (uid_t)a2, (gid_t)a3);
    case SYS_mmap2:      return (long) __mmap2((void *) a0, (size_t) a1, (int) a2, (int) a3, (int) a4, (off_t) a5);
    case SYS_munmap:     return __munmap((void *) a0, (size_t) a1);
    case SYS_creat:      return __creat((const char*) a0, (mode_t) a1);
    case SYS_faccessat:  return __faccessat((int) a0, (const char*) a1, (int) a2, (int) a3);
    case SYS_symlink:    return __symlink((const char*) a0, (const char*) a1);
    case SYS_symlinkat:  return __symlinkat((const char*) a0, (int) a1, (const char*) a2);
    case SYS_rename:     return __rename((const char*) a0, (const char*) a1);
    case SYS_rmdir:      return __rmdir((const char*) a0);
    case SYS_utime:      return __utime((const char*) a0, (const struct utimbuf*) a1);
    case SYS_utimensat:  return __utimensat((int) a0, (const char*) a1, (const struct timespec*) a2, (int) a3);
    case SYS_truncate:   return __truncate((const char *)a0, (off_t)a1);
    case SYS_ftruncate:  return __ftruncate((const char *)a0, (off_t)a1);
    case SYS_truncate64: return __truncate((const char *)a0, (off_t)a1);
    case SYS_ftruncate64:return __ftruncate((const char *)a0, (off_t)a1);
    case SYS_fsync:      return __fsync((int)a0);
    case SYS_fdatasync:  return __fdatasync((int)a0);
    case SYS_fsetxattr:  return __fsetxattr((int)a0, (const char *)a1, (const void *)a2, (size_t)a3, (int)a4);
    case SYS_exit_group: return __exit_group((int)a0);
    case SYS_pipe:       return __pipe((int *)a0);
    case SYS_pipe2:      return __pipe2((int *)a0, (int)a1);
    case SYS_socketcall: return __socketcall((int)a0, (unsigned long *)a1);
    case SYS_ioctl:      return __ioctl((int)a0, (int)a1, a2, a3, a4, a5);
    //case SYS_sigaction:  return __sigaction((int)a0, (const struct sigaction *)a1, (struct sigaction *)a2);
    case SYS_rt_sigaction:  return __rt_sigaction((int)a0, (const struct sigaction *)a1, (struct sigaction *)a2, (int)a3);
    //case SYS_select:     return __select((int)a0, (fd_set *)a1, (fd_set *)a2, (fd_set *)a3, (struct timeval *)a4);
    default:             break;
    }

    return vdso_invoke(num, a0, a1, a2, a3, a4, a5);
}

