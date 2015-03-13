#pragma once
#include <config/config.h>
#include <sys/types.h>
#include <sys/fs/fs.h>
#include <utilities/tsc.h>

#define REMOTE_EXEC ENABLE_REMOTE_EXEC

#ifdef __cplusplus
extern "C" {
#endif

// Scheduling
void fosInitSched();
void fosPreforkSched();
void fosParentSched();
void fosChildSched();
void fosNumaSched(InodeType inode, size_t filesize);
int fosExec(const char*filename, char *const argv[], char* const envp[]);

static uint64_t g_time_since_last;

static inline void fosClientLastMessage()
{
    g_time_since_last = rdtscll();
}


static inline uint64_t fosClientLastMessageTime()
{
    return g_time_since_last;
}

static inline void fosServerSleep()
{
#if ENABLE_SCHEDULER_RR
    sched_yield();
    return;
#endif

    uint64_t now = rdtscll();
    uint64_t diff = now - g_time_since_last;
    uint64_t thresh = 50000;
    if(diff > thresh)
    {
#if ENABLE_HARE_WAKE
        if(syscall(__nr_HARE,4) == 0)
        {
            //if the sleep call returns zero, that means that there
            //isn't anything else running -> as a consequence, go ahead
            //and spin for a bit
            hare_last();
        }
#else
        sched_yield();
#endif
    } else {
        sched_yield();
//        usleep(0);
    }
}

static inline void fosClientSleep()
{
#if ! ENABLE_SCHEDULER_RR
    uint64_t now = rdtscll();
    if(now - g_time_since_last > 5000)
#endif
        sched_yield();
}


#ifdef __cplusplus
}
#endif
