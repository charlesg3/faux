#define _GNU_SOURCE

#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <config/config.h>
#include <fd/fd.h>
#include <sys/sched/lib.h>
#include <sys/fs/lib.h>
#include <utilities/debug.h>
#include <utilities/tsc.h>
#include <messaging/dispatch_reset.h>
#include <messaging/channel_interface.h>
#include <name/name.h>

extern FosFileOperations fosFileOps;

extern int g_next_spawn_core;
extern Channel *establish_fs_chan(int i);

typedef int (*fcn)(int *(main) (int, char **, char **), int argc, char ** ubp_av, void (*init) (void), void (*fini) (void), void (*rtld_fini) (void), void (* stack_end));

bool g_faux_initted = false;


InodeType g_cwd_inode;
bool g_cwd_in;
pid_t g_original_pid;

extern Address g_fs_addr[CONFIG_SERVER_COUNT];

char *g_progname;

static void signal_cleanup(int sig)
{
    fsCloseFds();
    fosCleanup();
    dispatcherFree();
    _exit(0);
}

static void __atexit(void)
{
    LT(EXIT,"<-- %s", g_progname);
    fflush(stdout);
    fsCloseFds();
    fosCleanup();
}

void faux_init(char **av)
{
    enable_pcid();

    fosInitSched();
    g_progname = av[0];

    // Initialize file descriptors
    g_original_pid = getpid();
    fdInitialize();
    FosFDEntry *fd_entry;
    fdCreate(&fosFileOps, NULL, O_RDWR, &fd_entry);
    fdCreate(&fosFileOps, NULL, O_RDWR, &fd_entry);
    fdCreate(&fosFileOps, NULL, O_RDWR, &fd_entry);
    fdOnceOnly(STDIN_FILENO);
    fdOnceOnly(STDOUT_FILENO);
    fdOnceOnly(STDERR_FILENO);

    int j;
    for(j = 0; j < FD_COUNT; j++)
        fdRemove(j);

    atexit(__atexit);

    // Make sure we catch broken pipe signals
    // otherwise we don't clean up properly
    sigset_t mask;
    struct sigaction action;
    sigemptyset(&mask);
    action.sa_mask = mask;
    action.sa_handler = signal_cleanup;
    sigaction (SIGPIPE, &action, NULL);

    LT(START,"--> %s", g_progname);

    if(getenv("faux_initted"))
    {
        // copy the file descriptors
        char * fd_list = getenv("hare_fd_list");
        char * fd_list_copy = fd_list;
        unsigned int i;
        while(fd_list && *fd_list && sscanf(fd_list,"%x:", &i) > 0)
        {
            fd_list += 2; // ones and :
            if(i > 0xF)   fd_list++; // tens
            if(i > 0xFF)  fd_list++; // hundreds
            if(i > 0xFFF) fd_list++; // thousands

            char fd_name[128];
            sprintf(fd_name, "hare_fd_%d", i);
            char *fd_value= getenv(fd_name);
            assert(fd_value);

            // if there is no fd and what not set, then copy
            // over an underlying fd
            if(strstr(fd_value, "::::"))
            {
                int external_fd;
                sscanf(fd_value, "%d::::", &external_fd);
                fdSetExternal(i, external_fd);
            }
            else
            {
                int internal_fd;
                fd_t fd;
                InodeType inode;

                sscanf(fd_value, "%d:%llx:%llx@%d:%x", &internal_fd,
                        (long long unsigned int *)&fd, &inode.id, &inode.server, &inode.type);
                void *file;
                int retval = fosFileDescriptorCopy(&file, fd, inode);

                assert(file);

                if (retval) {
                    fprintf(stderr, "%s:%d - error duping file on start.\n", __FILE__, __LINE__);
                    assert(false);
                }

                int new_internal_fd = fdCreate(&fosFileOps, file, 0, &fd_entry);

                if (new_internal_fd < 0)
                    assert(false);

                assert(fd_entry->private_data);

                fdSetInternal(i, new_internal_fd);
            }

            unsetenv(fd_name);
        }

        // obtain cached fs addresses that have been established
        for(i = 0; i < CONFIG_SERVER_COUNT; i++)
        {
            char addr_name[128];
            char *addr_value;
            sprintf(addr_name, "hare_fs_addr_%d", i);
            addr_value = getenv(addr_name);
            if(addr_value)
            {
                sscanf(addr_value, "%llx", &g_fs_addr[i]);
                unsetenv(addr_name);
            }
        }

        if(fd_list_copy) unsetenv("hare_fd_list");

        // copy global vars (g_cwd_inode / g_cwd_in)
        char *g_env_val = getenv("hare_cwd");
        sscanf(g_env_val, "%llx@%d:%u:%d", &g_cwd_inode.id, &g_cwd_inode.server, (unsigned int *)&g_cwd_in, (int *)&g_cwd_inode.type);

#if ENABLE_SCHEDULING_RAND
    // for build_linux
    g_next_spawn_core = (rand() ^ rdtscll() ^ g_process) % CONFIG_SERVER_COUNT;
#elif ENABLE_SCHEDULING_NONFS
    if(CONFIG_FS_SERVER_COUNT < CONFIG_SERVER_COUNT)
        g_next_spawn_core = ((rand() ^ rdtscll() ^ g_process) % (CONFIG_SERVER_COUNT - CONFIG_FS_SERVER_COUNT)) + CONFIG_FS_SERVER_COUNT;
#elif ENABLE_SCHEDULING_NONFS_SEQ
    g_next_spawn_core++;
    if(g_next_spawn_core >= CONFIG_SERVER_COUNT)
        g_next_spawn_core = CONFIG_FS_SERVER_COUNT;
    if(g_next_spawn_core < CONFIG_FS_SERVER_COUNT)
        g_next_spawn_core = CONFIG_FS_SERVER_COUNT;
#endif
        establish_fs_chan(g_next_spawn_core);
    } else {
        g_cwd_inode.server = 0;
        g_cwd_inode.id = 0;
        g_cwd_inode.type = TYPE_DIR;
        g_cwd_in = true;

        // reserve stdin / stdout / stderr for underlying fs
        fdSetExternal(0, 0);
        fdSetExternal(1, 1);
        fdSetExternal(2, 2);

        for(int i = 0; i < CONFIG_SERVER_COUNT; i++)
        {
            if(!g_fs_addr[i])
                while(!name_lookup(FS_NAME + i, &g_fs_addr[i]))
                    ;
        }

        setenv("faux_initted", "true", true);
/*
        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        CPU_SET(g_next_spawn_core + 1, &cpu_set);
        sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
*/
#if ENABLE_SCHEDULER_RR
    struct sched_param params;
    params.sched_priority = 1;
    int ret = sched_setscheduler(0, SCHED_RR, &params);
    if(ret != 0)
        fprintf(stderr, "error setting scheduler.\n");
#endif

#if ENABLE_SCHEDULING_RAND
    // for build_linux
    g_next_spawn_core = (rand() ^ rdtscll() ^ g_process) % CONFIG_SERVER_COUNT;
#elif ENABLE_SCHEDULING_NONFS
    if(CONFIG_FS_SERVER_COUNT < CONFIG_SERVER_COUNT)
        g_next_spawn_core = ((rand() ^ rdtscll() ^ g_process) % (CONFIG_SERVER_COUNT - CONFIG_FS_SERVER_COUNT)) + CONFIG_FS_SERVER_COUNT;
#elif ENABLE_SCHEDULING_NONFS_SEQUENTIAL
    g_next_spawn_core++;
    if(g_next_spawn_core >= CONFIG_SERVER_COUNT)
        g_next_spawn_core = CONFIG_FS_SERVER_COUNT;
    if(g_next_spawn_core < CONFIG_FS_SERVER_COUNT)
        g_next_spawn_core = CONFIG_FS_SERVER_COUNT;
#endif
    }

    g_faux_initted = true;

    dispatchSetNotifyCallback(fosNotifications);

#if ENABLE_BUFFER_CACHE
    fosLoadBufferCache();
#endif
}

int __libc_start_main(int * (*main) (int, char **, char **), int argc, char ** ubp_av, void (*init) (void), void (*fini) (void), void (*rtld_fini) (void), void (* stack_end))
{
    faux_init(ubp_av);
    fcn start = NULL;
    *(void **)(&start) = dlsym(RTLD_NEXT, "__libc_start_main");
    return (*start)(main, argc, ubp_av, init, fini, rtld_fini, stack_end);
}

