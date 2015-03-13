#define _GNU_SOURCE

#include <stdio.h>
#include <linux/unistd.h>

#include <sys/fs/lib.h>
#include <system/syscall.h>
#include <messaging/dispatch_reset.h>

void __exit_group(int status)
{
    fsCloseFds();
    fosCleanup();
    dispatcherFree();
    real_exit_group(status);
}

