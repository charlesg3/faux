#pragma once

#include <sys/wait.h>
#include <stdio.h>
#include <utilities/debug.h>
#include <execinfo.h>

static inline void backtrace() {
    void *trace_ptrs[100];
    char strs[100][128];
    char *argv[100];
    int count = backtrace(trace_ptrs, 100);

    /* 1 - This version doesn't give much information in the output,
     * use -rdynamic in link step for more info but still not
     * satisfactory -nzb */

    /* char **funcNames = backtrace_symbols(trace_ptrs, count); */
    /* free(funcNames); */
    
    /* 2 - This version requires exec'ing another program */
    argv[0] = "addr2line";
    argv[1] = "-f";
    argv[2] = "-p";
    sprintf(strs[3], "-e/proc/%d/exe", getpid());
    argv[3] = strs[3];
    argv[4] = "-a";
    for (int i = 0; i < count; i++) {
        sprintf(strs[i+5], "%p", trace_ptrs[i]);
        argv[i+5] = strs[i+5];
    }
    argv[count+5] = NULL;

    if (fork()) {
        wait(NULL);
    } else {
        execvp("addr2line", argv);
    }
}
