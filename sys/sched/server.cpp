#include <stddef.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <iostream>
#include <sstream>

#include <linux/unistd.h>
#include <linux/futex.h>

#include <config/config.h>
#include <messaging/dispatch.h>
#include <messaging/channel.h>
#include <messaging/channel_interface.h>

#include <sys/sched/interface.h>

#include <name/name.h>
#include "server.hpp"
#include "marshall.hpp"

#include <utilities/tsc.h>
#include <utilities/debug.h>

#include <system/syscall.h>

using namespace std;
using namespace fos;
using namespace sched;

//static int g_socket_map[40] = {0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3};

SchedServer *SchedServer::g_ss = NULL;
unsigned int SchedServer::id;

static bool g_child_exited = false;

static void child_exit_signal(int sig)
{
    g_child_exited = true;
}

static void pipe_signal(int sig)
{
//    fprintf(stderr, "pipe signal\n");
}

SchedServer::~SchedServer()
{
}

static inline void futex(volatile int *var, int op, int arg)
{
    syscall(SYS_futex, var, op, arg, NULL, NULL, 0);
}

void ss_idle(void *arg)
{
    SchedServer & ss = SchedServer::ss();
    ss.idle();
}

SchedServer::SchedServer(int _id)
: m_id(_id)
, m_master(_id == 0)
, m_children()
{
    assert(g_ss == NULL);
    g_ss = this;

    if(CONFIG_FS_SERVER_COUNT < CONFIG_SERVER_COUNT && id >= CONFIG_FS_SERVER_COUNT)
        usleep(50000);

    dispatchRegister(EXEC, sched::exec);
    dispatchRegisterIdle(ss_idle, NULL);

    // Catch children exits
    sigset_t mask;
    struct sigaction action;
    sigemptyset(&mask);
    action.sa_mask = mask;
    action.sa_handler = child_exit_signal;
    sigaction (SIGCHLD, &action, NULL);

    sigemptyset(&mask);
    action.sa_mask = mask;
    action.sa_handler = pipe_signal;
    sigaction (SIGPIPE, &action, NULL);

    // bind to core(id)
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
 //   if(CONFIG_SERVER_COUNT > 8)
        CPU_SET(id, &cpu_set);
 //   else
 //       CPU_SET(id * 4, &cpu_set);

    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);


#if ENABLE_SCHEDULER_RR
    struct sched_param params;
    params.sched_priority = 1;
    int ret = sched_setscheduler(0, SCHED_RR, &params);
    if(ret != 0)
        fprintf(stderr, "error setting scheduler.\n");
#endif

}

void cleanup_child(int control_fd, volatile struct exec_shared_t *exec_shared, int rc)
{
    int exec_rc = exec_shared->exec_rc;
    int exec_errno = exec_shared->exec_errno;
    munmap((void *)exec_shared, sizeof(*exec_shared));
    int type = exec_rc == 0 ? CHILD_EXIT : EXEC_ERROR;
    if(type == CHILD_EXIT)
        exec_rc = WEXITSTATUS(rc);

    struct ExecReply exit_reply;
    exit_reply.type = type;
    exit_reply.retval = exec_rc;
    exit_reply.errno_reply = exec_errno;

    int write_ret __attribute__((unused)) = write(control_fd, &exit_reply, sizeof(exit_reply));
//    assert(write_ret == sizeof(exit_reply));

    close(control_fd);
}


void SchedServer::exec(int pid, const char *filename, char *args, char *envs, void (*callback)(void *, int type, int rc, int errno_reply), void *data)
{
    // copy the arguments...
    int ac = 0;
    int ec = 0;
    char *p = args;
    while(p + 1 < envs)
    {
        ac++;
        p += strlen(p) + 1;
    }

    p++;

    while(*p)
    {
        ec++;
        p += strlen(p) + 1;
    }

    ec++;

    char *data_copy = (char *)malloc((p - filename) + 1024);
    assert(data_copy);
    strcpy(data_copy, filename);
    char *pdest = data_copy + strlen(filename) + 1;

    char ** av = (char **) malloc(sizeof(char*) * (ac + 1));
    assert(av);
    char ** ev = (char **) malloc(sizeof(char*) * (ec + 1));
    assert(ev);

    p = args;
    for(int i = 0; i < ac; i++, p += (strlen(p) + 1))
    {
        av[i] = pdest;
        strcpy(pdest, p);
        pdest += strlen(p) + 1;
    }

    av[ac] = 0;

    p++;

    for(int i = 0; i < ec; i++, p += (strlen(p) + 1))
    {
        ev[i] = pdest;
        strcpy(pdest, p);
        pdest += strlen(pdest) + 1;
    }

    ev[ec -1] = 0;

    volatile struct exec_shared_t *exec_shared = (struct exec_shared_t *) mmap(0, sizeof(*exec_shared), PROT_READ|PROT_WRITE, 
                                    MAP_SHARED|MAP_ANONYMOUS, 0, 0);
    exec_shared->exec_rc = 0;
    exec_shared->child_started = false;
    exec_shared->parent_started = false;
    char *hare_build = getenv("HARE_BUILD");

    pid_t child = fork();
    if(child == 0)
    {
        enable_pcid();
        FILE *newstream;
        char pipe_str[1024];

        umask(0022);

        // replace stdin / stdout / stderr with named pipes
        snprintf(pipe_str, 1024, "%s/fifo.%d.1", hare_build, pid);
        newstream = freopen(pipe_str, "w", stdout);
        assert(newstream);

        snprintf(pipe_str, 1024, "%s/fifo.%d.2", hare_build, pid);
        newstream = freopen(pipe_str, "w", stderr);
        assert(newstream);

        exec_shared->child_started = true;
        futex(&exec_shared->child_started, FUTEX_WAKE, 1);

        snprintf(pipe_str, 1024, "%s/fifo.%d.0", hare_build, pid);
        newstream = freopen(pipe_str, "r", stdin);
        assert(newstream);

        // clear out any affinity
        /*
        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);
        CPU_SET(1, &cpu_set);
        for(int i = 0; i < 40; i++)
            CPU_SET(i, &cpu_set);
        sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
            */

        futex(&exec_shared->parent_started, FUTEX_WAIT, false);

        munmap((void *)exec_shared, sizeof(*exec_shared));
        int exec_rc = execve(data_copy, av, ev);
        assert(false);

        // execve must have failed, now cleanup
        exec_shared->exec_errno = errno;
        exec_shared->exec_rc = exec_rc;
        syscall(__NR_exit, 0);
    } else {
        ChildInfo *ci = new ChildInfo();
        ci->shared = exec_shared;
        ci->control_fd = -1;

        //        pid_t watcher = fork();
        //        if(watcher == 0)
        {
            char control_pipe[1024];
            snprintf(control_pipe, 1024, "%s/fifo.%d.c", hare_build, pid);
            int control_fd;
reopen_control_fd:
            control_fd = open(control_pipe, O_WRONLY | O_CLOEXEC);
            if(control_fd < 0)
            {
                if(errno == EINTR)
                    goto reopen_control_fd;
                perror("control fd:");
            }

            assert(control_fd >= 0);

            ci->control_fd = control_fd;
            m_children[child] = ci;

            // wait for the child to open the named pipes
            futex(&exec_shared->child_started, FUTEX_WAIT, false);

            // notify the spawner that the child has started
            int write_ret = write(control_fd, "open", 5);
            assert(write_ret == 5);

            exec_shared->parent_started = true;
            futex(&exec_shared->parent_started, FUTEX_WAKE, 1);
            /*
               int rc;
               waitpid(child, &rc, 0);
               cleanup_child(control_fd, exec_shared, rc);
               syscall(__NR_exit, 0);
               */
            //        } else {
    }
    }
}

void show_mem()
{
    static uint64_t last_mem;
    size_t mem = 0;
    long rss = 0L;
    FILE* fp = NULL;
    if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
        assert(false);
    if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
    {
        fclose( fp );
        assert(false);
    }
    fclose( fp );

    mem = (size_t)rss * (size_t)sysconf( _SC_PAGESIZE);

//    struct rusage usage;
//    getrusage(RUSAGE_SELF, &usage);

    if(mem != last_mem)
    {
        fprintf(stderr, "memory: %d\n", mem);
        last_mem = mem;
    }
}

extern void fs_idle();

void SchedServer::idle()
{
    if(m_id < CONFIG_FS_SERVER_COUNT)
        fs_idle();

    if(!m_children.size() || !g_child_exited)
        return;

    g_child_exited = false;

/*
    static uint64_t time_since_last;
    uint64_t now = rdtsc64();
    uint64_t diff = now - time_since_last;
    if(diff > 200000)
        time_since_last = now;
        //show_mem();
    }
    */

wait_again:

    int rc;
    pid_t child = waitpid(-1, &rc, WNOHANG);


    if(child == -1) return;
    if(child == 0) return;

    // wait for child to finish
    auto const & ci_i = m_children.find(child);
    assert(ci_i != m_children.end());
    auto * ci = ci_i->second;

    auto exec_shared = ci->shared;
    int control_fd = ci->control_fd;
    assert(control_fd > 0);

    m_children.erase(child);
    delete ci;

    // notify the spanwer of child return
    cleanup_child(control_fd, exec_shared, rc);

    goto wait_again;
}

void SchedServer::childExit()
{
    return;
    int rc;
    pid_t child = wait(&rc);

    // wait for child to finish
    auto const & ci_i = m_children.find(child);
    assert(ci_i != m_children.end());
    auto * ci = ci_i->second;

    auto exec_shared = ci->shared;
    int control_fd = ci->control_fd;
    assert(control_fd > 0);

    m_children.erase(child);
    delete ci;

    // notify the spanwer of child return
    cleanup_child(control_fd, exec_shared, rc);
}

