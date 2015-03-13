#define VDSOWRAP

#include <cassert>
#include <cerrno>
#include <iostream>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/unistd.h>

#include <config/config.h>

#include <fd/fd.h>
#include <sys/fs/lib.h>
#include <sys/sched/lib.h>
#include <sys/sched/interface.h>
#include <utilities/tsc.h>
#include <utilities/debug.h>
#include <name/name.h>
#include <messaging/dispatch.h>
#include <messaging/dispatch_reset.h>
#include <system/syscall.h>

#define PIPE_CHUNK_SIZE 8192

namespace fos { namespace sched { 
    namespace app {}
}}

using namespace std;
using namespace fos;
using namespace sched;
using namespace app;

extern "C" {
extern int __select(int nfds, fd_set *, fd_set *, fd_set *, struct timeval *);
#if ENABLE_SCHEDULING_NONFS
int g_next_spawn_core = CONFIG_FS_SERVER_COUNT;
#elif ENABLE_SCHEDULING_NONFS_SEQ
int g_next_spawn_core = CONFIG_FS_SERVER_COUNT;
#else
int g_next_spawn_core;
#endif
}

enum
{
    FD_CAN_WRITE,
    FD_CANT_WRITE,
    FD_ERROR
};

// Channel management
extern Channel *establish_fs_chan(int i);

static void signal_handler(int sig)
{
    fprintf(stderr, "signal: %d\n", sig);
}

static inline int can_write(int fd)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLOUT | POLLERR | POLLHUP;
    poll(&pfd, 1, 0);
    if(pfd.revents & POLLOUT) return FD_CAN_WRITE;
    else if(pfd.revents & POLLERR || pfd.revents & POLLHUP)
        return FD_ERROR;

    return FD_CANT_WRITE;
}

// RPC Library Calls...
int fosExec(const char*filename, char *const argv[], char* const envp[])
{

#if ! REMOTE_EXEC
    dispatcherFree();
    return real_execve(filename, argv, envp);
#endif

    int ret;
    Channel *chan = establish_fs_chan(g_next_spawn_core);
    size_t request_size = sizeof(ExecRequest) + strlen(filename) + 1;


    //copy the args
    for(int i = 0; argv[i]; i++)
        request_size += strlen(argv[i]) + 1;

    for(int i = 0; envp[i]; i++)
        request_size += strlen(envp[i]) + 1;

    request_size += 2;

    ExecRequest* request = (ExecRequest *) dispatchAllocate(chan, request_size);
    strcpy(request->filename, filename);

    char *p = request->filename + strlen(request->filename);
    *p++ = 0;
    for(int i = 0; argv[i]; i++)
    {
        strcpy(p, argv[i]);
        p += strlen(argv[i]);
        *p++ = 0;
    }

    *p++ = 0;

    request->envp_offset = p - request->filename;

    bool found_stdin = false;
    bool found_stdout = false;
    bool found_stderr = false;
    for(int i = 0; envp[i]; i++)
    {
        strcpy(p, envp[i]);
        if(strncmp(p, "hare_fd_", strlen("hare_fd_")) == 0)
        {
            char *fd_str = p + strlen("hare_fd_");
            if(*fd_str != 'l')
            {
                char fd_str_copy[1024];
                snprintf(fd_str_copy, 1024, "%s", fd_str);
                char *equals = strchr(fd_str_copy, '=');
                *equals = '\0';
                equals++;
                int hare_fd = atoi(fd_str_copy);
                if(hare_fd == 0 && strcmp(equals + 1, "::::"))
                    found_stdin = true;
                if(hare_fd == 1 && strcmp(equals + 1, "::::"))
                    found_stdout = true;
                if(hare_fd == 2 && strcmp(equals + 1, "::::"))
                    found_stderr = true;
            }
        }
        p += strlen(envp[i]) + 1;
    }

    *p++ = 0;

    request->pid = getpid();

    // create the fifo pipe
    char stdin_pipe[1024];
    char *hare_build = getenv("HARE_BUILD");
    int pid = getpid();
    snprintf(stdin_pipe, 1024, "%s/fifo.%d.0", hare_build, pid);
    ret = mkfifo(stdin_pipe, 0777);
    assert(ret == 0);

    char stdout_pipe[1024];
    snprintf(stdout_pipe, 1024, "%s/fifo.%d.1", hare_build, pid);
    ret = mkfifo(stdout_pipe, 0777);
    assert(ret == 0);

    char stderr_pipe[1024];
    snprintf(stderr_pipe, 1024, "%s/fifo.%d.2", hare_build, pid);
    ret = mkfifo(stderr_pipe, 0777);
    assert(ret == 0);

    char control_pipe[1024];
    snprintf(control_pipe, 1024, "%s/fifo.%d.c", hare_build, pid);
    ret = mkfifo(control_pipe, 0777);
    assert(ret == 0);

    // send the request
    DispatchTransaction transaction = dispatchRequest(chan, EXEC, request, request_size);
    hare_wake(g_next_spawn_core);

    int control_pipe_fd = real_open(control_pipe, O_RDONLY, 0);
    int stdout_pipe_fd = real_open(stdout_pipe, O_RDONLY, 0);
    int stderr_pipe_fd = real_open(stderr_pipe, O_RDONLY, 0);

    char buf[PIPE_CHUNK_SIZE];
    int read_size;
    int write_size;
    read_size = real_read(control_pipe_fd, buf, PIPE_CHUNK_SIZE);
    assert(read_size == 5 && strcmp(buf, "open") == 0);

    int stdin_pipe_fd = real_open(stdin_pipe, O_WRONLY, 0);
    fd_set set;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    int max_fd = stdout_pipe_fd;
    if(stderr_pipe_fd > max_fd)
        max_fd = stderr_pipe_fd;
    if(control_pipe_fd > max_fd)
        max_fd = control_pipe_fd;

    int fd_count;
    bool stdin_valid = !found_stdin;
    bool stdout_valid = !found_stdout;
    bool stderr_valid = !found_stderr;
    bool control_valid = true;

    // Make sure we catch broken pipe signals
    // otherwise we don't clean up properly
#if 0
    sigset_t mask;
    struct sigaction action;
    sigemptyset(&mask);
    action.sa_mask = mask;
    action.sa_handler = signal_handler;
    sigaction (SIGTERM, &action, NULL);
    sigaction (SIGHUP, &action, NULL);
    sigaction (SIGCONT, &action, NULL);
    sigaction (SIGTTOU, &action, NULL);
    sigaction (SIGTTIN, &action, NULL);
    sigaction (SIGTSTP, &action, NULL);
    //sigaction (SIGINT, &action, NULL);
#endif
    signal(SIGPIPE, SIG_IGN);

//    StatusReply *reply;
//    size_t size = dispatchReceive(transaction, (void **) &reply);

//    assert(size == sizeof(*reply));
//    dispatchFree(transaction, reply);
    dispatcherFree();

    ExecReply exec_reply;
    bool exec_replied = false;

    while(stdout_valid || control_valid)
    {
        FD_ZERO(&set);
        if(stdout_valid) FD_SET(stdout_pipe_fd, &set);
        if(stderr_valid) FD_SET(stderr_pipe_fd, &set);
        if(stdin_valid)  FD_SET(fileno(stdin), &set);
        if(control_valid) FD_SET(control_pipe_fd, &set);

        fd_count = select(max_fd + 1, &set, NULL, NULL, NULL);

        if(fd_count > 0)
        {
            if(stdout_valid && FD_ISSET(stdout_pipe_fd, &set))
            {
                int cw = can_write(fileno(stdout));
                if(cw == FD_CAN_WRITE)
                {
                    read_size = real_read(stdout_pipe_fd, buf, PIPE_CHUNK_SIZE);
                    if(read_size > 0)
                    {
                        write_size = real_write(fileno(stdout), buf, read_size);
                        //                    assert(write_size == read_size);
                    }
                    else
                    {
                        stdout_valid = false;
                    }
                }
                else if(cw == FD_ERROR)
                {
                    stdout_valid = false;
                }
                else
                {
                    sched_yield();
                }
            }

            if(stderr_valid && FD_ISSET(stderr_pipe_fd, &set))
            {
                read_size = real_read(stderr_pipe_fd, buf, PIPE_CHUNK_SIZE);
                if(read_size > 0)
                {
                    write_size = real_write(fileno(stderr), buf, read_size);
//                    assert(write_size == read_size);
                }
                else
                    stderr_valid = false;
            }

            if(stdin_valid && FD_ISSET(fileno(stdin), &set))
            {
                int cw = can_write(stdin_pipe_fd);
                if(cw == FD_CAN_WRITE)
                {
                    read_size = real_read(fileno(stdin), buf, PIPE_CHUNK_SIZE);
                    if(read_size > 0)
                    {
                        write_size = real_write(stdin_pipe_fd, buf, read_size);
//                        assert(write_size == read_size);
                    }
                    else
                    {
                        real_close(stdin_pipe_fd);
                        stdin_valid = false;
                    }
                }
                else if(cw == FD_ERROR)
                {
                    real_close(stdin_pipe_fd);
                    stdin_valid = false;
                }
                else
                {
                    sched_yield();
                }

            }

            if(FD_ISSET(control_pipe_fd, &set))
            {
                read_size = real_read(control_pipe_fd, buf, PIPE_CHUNK_SIZE);
                if(read_size > 0)
                {
                    //NOTE: putting a fprintf(stderr, ...) here will break redirection test.
                    assert(read_size == sizeof(ExecReply));
                    memcpy(&exec_reply, buf, sizeof(ExecReply));
                    exec_replied = true;
                }
                else
                    control_valid = false;
            }
        }
        else if(fd_count < 0)
        {
            perror("select(): ");
            break;
        }
        else if(fd_count == 0)
            sched_yield();
    }


//    ExecReply *reply;
//    size_t size = dispatchReceive(transaction, (void **) &reply);

    assert(exec_replied);

    if(stdin_valid)
        real_close(stdin_pipe_fd);
    real_close(stdout_pipe_fd);
    real_close(stderr_pipe_fd);
    real_close(control_pipe_fd);

    int type = exec_reply.type;
    int rc = exec_reply.retval;
    int errno_reply = exec_reply.errno_reply;

    real_unlink(stdin_pipe);
    real_unlink(stdout_pipe);
    real_unlink(stderr_pipe);
    real_unlink(control_pipe);

    if(type == CHILD_EXIT)
    {
        syscall(__NR_exit, rc);
    }
    else
    {
        assert(false);
        errno = -errno_reply;
        return rc;
    }
}

// auxillary functions
int g_process = 0;

int g_core;
int g_cores = 40;
int g_socket;
int g_sockets=4;
int g_servers_per_socket;
int g_socket_index;
int g_dest_server;
int g_server_in_socket;
int g_socket_map[40] = {0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3};

static void init_cpu_info()
{
    g_core = sched_getcpu();
    g_socket = g_socket_map[g_core];
    g_sockets = 4;
    if(CONFIG_FS_SERVER_COUNT < g_sockets)
        g_servers_per_socket = 1;
    else
        g_servers_per_socket = CONFIG_FS_SERVER_COUNT / g_sockets;
}

void _setDest(int core)
{
    //g_core = sched_getcpu();
    g_core = core;
    g_socket = g_socket_map[core];
    g_socket_index = g_core / g_sockets;
    g_server_in_socket = g_socket_index % g_servers_per_socket;
    g_dest_server = g_socket + (g_server_in_socket * g_sockets);
    //if(CONFIG_FS_SERVER_COUNT < 4)
    g_dest_server %= CONFIG_FS_SERVER_COUNT;
}

static void pin(int socket)
{
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
//    CPU_SET(0 + socket, &cpu_set);
//    CPU_SET(4 + socket, &cpu_set);
//    CPU_SET(8 + socket, &cpu_set);
//    CPU_SET(12 + socket, &cpu_set);
//    CPU_SET(16 + socket, &cpu_set);
    CPU_SET(20 + socket, &cpu_set);
    CPU_SET(24 + socket, &cpu_set);
    CPU_SET(28 + socket, &cpu_set);
    CPU_SET(32 + socket, &cpu_set);
    CPU_SET(36 + socket, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
}

void fosInitSched()
{
#if ENABLE_SCHEDULER_RR
    struct sched_param params;
    params.sched_priority = 1;
    int ret = sched_setscheduler(0, SCHED_RR, &params);
    if(ret != 0)
        fprintf(stderr, "error setting scheduler.\n");
#endif

    init_cpu_info();

    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
#if ENABLE_SCHEDULING_STRIPE
    CPU_SET(g_process, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
    _setDest(g_process);
#endif

#if ENABLE_SCHEDULING_NON_SERVER
    for(int i = CONFIG_SERVER_COUNT; i < 40; i++)
        CPU_SET(i, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
    _setDest(sched_getcpu());
#endif

#if ENABLE_SCHEDULING_CORE0
    CPU_SET(0, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
    _setDest(0);
#endif

#if ENABLE_SCHEDULING_SOCKET0
//    CPU_SET(0, &cpu_set);
    CPU_SET(4 , &cpu_set);
    CPU_SET(8 , &cpu_set);
    CPU_SET(12, &cpu_set);
    CPU_SET(16, &cpu_set);
    CPU_SET(20, &cpu_set);
    CPU_SET(24, &cpu_set);
    CPU_SET(28, &cpu_set);
    CPU_SET(32, &cpu_set);
    CPU_SET(36, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
    _setDest(sched_getcpu());
#endif

#if ENABLE_SCHEDULING_SOCKET1
    CPU_SET(1, &cpu_set);
    CPU_SET(5 , &cpu_set);
    CPU_SET(9 , &cpu_set);
    CPU_SET(13, &cpu_set);
    CPU_SET(17, &cpu_set);
    CPU_SET(21, &cpu_set);
    CPU_SET(25, &cpu_set);
    CPU_SET(29, &cpu_set);
    CPU_SET(33, &cpu_set);
    CPU_SET(37, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
    _setDest(sched_getcpu());
#endif

#if ENABLE_SCHEDULING_NONE
    _setDest(sched_getcpu());
#endif

}

void fosPreforkSched()
{
    if((g_process + 1) > g_cores)
    {
        //g_process = CONFIG_SERVER_COUNT;
        g_process = 0;
    }
}


void fosParentSched()
{
    g_process++;
#if ENABLE_SCHEDULING_RAND
    // for build_linux
    g_next_spawn_core = (rand() ^ rdtscll() ^ g_process) % CONFIG_SERVER_COUNT;
#elif ENABLE_SCHEDULING_NONFS
    g_next_spawn_core = ((rand() ^ rdtscll() ^ g_process) % (CONFIG_SERVER_COUNT - CONFIG_FS_SERVER_COUNT)) + CONFIG_FS_SERVER_COUNT;
#elif ENABLE_SCHEDULING_NONFS_SEQ
    g_next_spawn_core++;
    if(g_next_spawn_core >= CONFIG_SERVER_COUNT)
        g_next_spawn_core = CONFIG_FS_SERVER_COUNT;
    if(g_next_spawn_core < CONFIG_FS_SERVER_COUNT)
        g_next_spawn_core = CONFIG_FS_SERVER_COUNT;
#else
    // for creates,etc ...
    g_next_spawn_core++;
    g_next_spawn_core %= CONFIG_SERVER_COUNT;
#endif

    establish_fs_chan(g_next_spawn_core);
}


void fosChildSched()
{
    srand(rdtscll() ^ g_process ^ getpid() << 16);
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
    g_next_spawn_core = ((rand() ^ rdtscll() ^ g_process) % (CONFIG_SERVER_COUNT - CONFIG_FS_SERVER_COUNT)) + CONFIG_FS_SERVER_COUNT;
#elif ENABLE_SCHEDULING_NONFS_SEQ
    g_next_spawn_core++;
    if(g_next_spawn_core >= CONFIG_SERVER_COUNT)
        g_next_spawn_core = CONFIG_FS_SERVER_COUNT;
    if(g_next_spawn_core < CONFIG_FS_SERVER_COUNT)
        g_next_spawn_core = CONFIG_FS_SERVER_COUNT;
#else
    // for creates,etc ...
    g_next_spawn_core = g_process % CONFIG_SERVER_COUNT;
#endif

    establish_fs_chan(g_next_spawn_core);

    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
#if ENABLE_SCHEDULING_STRIPE
    CPU_SET(g_process, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
    _setDest(g_process);
#endif

#if ENABLE_SCHEDULING_SOCKET0
//    CPU_SET(0, &cpu_set);
    CPU_SET(4 , &cpu_set);
    CPU_SET(8 , &cpu_set);
    CPU_SET(12, &cpu_set);
    CPU_SET(16, &cpu_set);
    CPU_SET(20, &cpu_set);
    CPU_SET(24, &cpu_set);
    CPU_SET(28, &cpu_set);
    CPU_SET(32, &cpu_set);
    CPU_SET(36, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
    _setDest(sched_getcpu());
#endif

#if ENABLE_SCHEDULING_SOCKET1
    CPU_SET(1, &cpu_set);
    CPU_SET(5 , &cpu_set);
    CPU_SET(9 , &cpu_set);
    CPU_SET(13, &cpu_set);
    CPU_SET(17, &cpu_set);
    CPU_SET(21, &cpu_set);
    CPU_SET(25, &cpu_set);
    CPU_SET(29, &cpu_set);
    CPU_SET(33, &cpu_set);
    CPU_SET(37, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
    _setDest(sched_getcpu());
#endif

#if ENABLE_SCHEDULING_NON_SERVER
    // inherited from parent
#endif

#if ENABLE_SCHEDULING_CORE0
    CPU_SET(0, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
    _setDest(0);
#endif

#if ENABLE_SCHEDULING_NONE
    _setDest(sched_getcpu());
#endif
}

void fosNumaSched(InodeType inode, size_t filesize)
{
#if ENABLE_NUMA_SCHEDULE
    if(filesize >= 1024 * 4)
    {
        int core = sched_getcpu();
        int socket = g_socket_map[core];
        int server_socket = g_socket_map[inode.server];
        if(socket != server_socket)
            pin(server_socket);

        _setDest();
    }
#endif
}
