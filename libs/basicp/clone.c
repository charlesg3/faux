#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/types.h>

#include <fd/fd.h>
#include <stdio.h>
#include <messaging/dispatch_reset.h>
#include <sys/fs/lib.h>
#include <sys/sched/lib.h>
#include <system/syscall.h>
#include <name/name.h>
#include <sys/fs/buffer_cache_log.h>

#ifdef VDSOWRAP
long __clone(unsigned long clone_flags, unsigned long newsp, int *parent_tidptr, int *child_tidptr, int tls_val)
{
    fsFlattenFds();
    fsCloneFds();
    fosPreforkSched();

    long tid = real_clone(clone_flags, newsp, parent_tidptr, child_tidptr, tls_val);
    if (tid) {
        fosParentSched();
        LT(CLONE, "%d -> %ld", getpid(), tid);
        return tid;
    } else {
        name_reset();
        fsReset();
        dispatchReset();
        fosChildSched();
        fosDirCacheClear();
        fosBufferCacheLogReset();
        return tid;
    }
}
#endif

