#define _GNU_SOURCE

#include <assert.h>
#include <error.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fd/fd.h>
#include <messaging/dispatch_reset.h>
#include <sys/sched/lib.h>
#include <sys/fs/lib.h>
#include <system/syscall.h>
#include <utilities/debug.h>

#include <messaging/channel_interface.h>


extern InodeType g_cwd_inode;
extern bool g_cwd_in;

extern Address g_fs_addr[CONFIG_SERVER_COUNT];

extern void *image_load (char *elf_start, unsigned int size);

int __execve(const char* filename, char* const argv[], char* const envp[])
{
    // count the number of entries in the environment excluding file
    // descriptors and current working directory
    int envc = 0;
    for (int i = 0; envp[i]; i++) {
        if (strstr("hare_", envp[i]) == NULL)
            envc++;
    }

    // count the number of active file descriptor +2 for current working
    // directory and add to the number of entries in the environment
    for (int i = 0; i < FD_COUNT; i++)
        if (fdIsInternal(i) || fdIsExternal(i)) envc += 1;

    for (int i = 0; i < CONFIG_SERVER_COUNT; i++)
        if (g_fs_addr[i]) envc += 1;

    // pass on the names that we've established

    envc += 3; // cwd, cwd_in and fd_list

    // XXX: the +1 is required to store NULL at the end of new_envp -- siro
    char** new_envp = (char**) malloc(sizeof(char*) * (envc + 1));
    assert(new_envp);

#if ENABLE_DBG_CLIENT_EXEC
    char args_str[4096];
    char **arg_p = (char **)argv;
    strcat(args_str, "[ ");
    for (int i = 0; argv[i]; i++)
    {
        strcat(args_str, *(arg_p)++);
        strcat(args_str, " ");
    }
    strcat(args_str, "]");
    LT(EXEC, "execing: %s %s", filename, args_str);
#endif

    // Internally close the file descriptors (the ones that we manage)
    // that have the FD_CLOEXEC flag set.
    for (int fd = 2; fd < FD_COUNT; fd++)
        if (fdIsInternal(fd) && (fcntl(fd, F_GETFD) & FD_CLOEXEC))
            __close_int(fd);

    fsFlattenFds();

    int entries = 0;
    char fd_list[4096];
    char *fd_list_p = fd_list;
    int retval __attribute__((unused));
    fd_list[0] = '\0';
    // prepare the entries for file descriptors
    for (int i = 0; i < FD_COUNT; i++) {
        if (fdIsInternal(i)) {
            FosFDEntry* file = fdGet(fdInt(i));
            fd_t fd = fosFileFd(file->private_data);
            InodeType inode = fosFileIno(file->private_data);

            assert(file); // sanity check
            char* entry = NULL;
            retval = asprintf(&entry, "hare_fd_%d=%d:%llx:%llx@%d:%x", i, fdInt(i), fd, inode.id, inode.server, inode.type);
            assert(retval != -1);
            new_envp[entries++] = entry;

            // add it to the list
            int printed = sprintf(fd_list_p, "%x:", i);
            assert(printed > 0);
            fd_list_p += printed;
        }
        else if(fdIsExternal(i))
        {
            char* entry = NULL;
            retval = asprintf(&entry, "hare_fd_%d=%d::::", i, fdExt(i));
            assert(retval != -1);
            new_envp[entries++] = entry;

            // add it to the list
            int printed = sprintf(fd_list_p, "%x:", i);
            assert(printed > 0);
            fd_list_p += printed;
        }
    }

    if(fd_list_p != fd_list)
    {
        char* entry = NULL;
        retval = asprintf(&entry, "hare_fd_list=%s", fd_list);
        assert(retval != -1);
        new_envp[entries++] = entry;
    }

    // entries for current working directory
    {
        char* entry = NULL;
        retval = asprintf(&entry, "hare_cwd=%llx@%d:%u:%d", (unsigned long long) g_cwd_inode.id, g_cwd_inode.server,
                g_cwd_in, g_cwd_inode.type);
        assert(retval != -1);
        new_envp[entries++] = entry;
    }


    // entries for addresses of servers we've communicated established
    for (int i = 0; i < CONFIG_SERVER_COUNT; i++)
    {
        if(g_fs_addr[i])
        {
            char* entry = NULL;
            retval = asprintf(&entry, "hare_fs_addr_%d=%llx", i, g_fs_addr[i]);
            assert(retval != -1);
            new_envp[entries++] = entry;
        }
    }

    // copy the original entries of the environment at the end of new_envp to
    // speedup lookups to Hare variables
    for (int i = 0; envp[i]; i++)
        new_envp[entries++] = envp[i];

    // end new_envp
    new_envp[entries] = NULL;

#if 0
    fprintf(stderr, "execve():\n\tfilename = %s\n\targv =\n", filename);
    for (int i = 0; argv[i]; i++)
        fprintf(stderr, "\t\t%s\n", argv[i]);
    fprintf(stderr, "\tenvp =\n");
    for (int i = 0; new_envp[i]; i++)
        fprintf(stderr, "\t\t%s\n", new_envp[i]);
#endif

    if (fosPathExcluded(filename)) {
        if (access(filename, F_OK) == 0) {
        retval = fosExec(filename, argv, new_envp);
        }
        else
        {
#if REMOTE_EXEC
            errno = ENOENT;
#else
            retval = ENOENT;
#endif
        }

        // free heap allocated memory whenever an error occurs
        for (int i = 0; i < entries; i++) {
            if (strstr("hare_", new_envp[i]) != NULL)
                free(new_envp[i]);
        }
        free(new_envp);

#if REMOTE_EXEC
        return errno;
#else
        return retval;
#endif
    }

    // XXX: I didn't touch the following code -- siro

    int i = 0;
    int argc = 0;
    while(argv[i++]) ++argc;

    // If it is an elf, then try to load that
    static char buf[1048576];
    int elf = open(filename, O_RDONLY);
    assert(elf >= 0);
    ssize_t binsize = read(elf, buf, sizeof(buf));
    close(elf);
    if(buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F')
    {
        // write the file out to the fs and run it from there...

        // copy file to external

        // construct the path
        char *builddir = getenv("HARE_BUILD");
        assert(builddir);
        char outpath[MAX_FNAME_LEN];
        char *slash = strrchr(argv[0], '/');
        snprintf(outpath, MAX_FNAME_LEN, "%s/%s", builddir, slash ? slash + 1 : argv[0]);

        // write the file
        if (access(outpath, F_OK) != 0) {
            char outpath_local[MAX_FNAME_LEN];
            snprintf(outpath_local,  MAX_FNAME_LEN, "%s/%s.%d", builddir, slash ? slash + 1 : argv[0], getpid());
            int out = open(outpath_local, O_RDWR | O_CREAT | O_TRUNC, 00755);
            if(out < 0)
            {
                PS("couldn't open output binary: %s error: %s\n", outpath_local, strerror(-out));
                exit(-1);
            }
            if(write(out, buf, binsize) != binsize) assert(false);
            close(out);
            rename(outpath_local, outpath);
        }

        filename = outpath; //setup environment

        // exec file
        retval = fosExec(filename, argv, new_envp);
        error(0, errno, "execve(%s)", filename);
        return retval;
    }

    // If it's not an elf then just try to run it with bash
    // +2 because we insert bash at beginning and need a null at end
    char **new_argv = (char **)malloc(argc * sizeof(char *) + 2);
    i = 0;
    while(argv[i])
    {
        new_argv[i + 1] = argv[i];
        ++i;
    }
    new_argv[0] = "bash";
    new_argv[i+1] = NULL;
    filename = "/bin/bash";

    retval = fosExec(filename, new_argv, new_envp);
    error(0, errno, "execve(%s)", new_argv[1]);
    return retval;
}

#ifndef VDSOWRAP

int execve(const char* filename, char* const argv[], char* const envp[])
{
    int retval = __execve(filename, argv, envp);
    if (retval) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

#endif
