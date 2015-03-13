#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>

#include <sys/fs/lib.h>
#include <system/syscall.h>
#include <utilities/debug.h>

static __sighandler_t g_handlers[64] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int g_interrupted = false;

static void __trampoline_handler(int signum)
{
    if(g_handlers[signum])
        g_handlers[signum](signum);

    g_interrupted = true;
}


int __sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    if(signum >= 0 && signum < 64 && act)
        g_handlers[signum] = act->sa_handler;
    return real_sigaction(signum, act, oldact);
}

int __rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact, int size)
{
    if(signum >= 0 && signum < 64 && act)
        g_handlers[signum] = act->sa_handler;

    // see if child is trying to set a handler, if so then re-write to our
    // own handler so that we can intercept the signal.
    struct sigaction *act_write = (struct sigaction *)act;
    if(act && (signum == SIGCHLD || signum == SIGALRM) && act->sa_handler != SIG_IGN && act->sa_flags == 0)
        act_write->sa_handler = __trampoline_handler;

    return real_rt_sigaction(signum, act, oldact, size);
}
