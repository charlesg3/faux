#include <cassert>
#include <stddef.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sched.h>

#include <sys/fs/buffer_cache_log.h>
#include <fd/fd.h>
#include <sys/fs/fs.h>
#include <system/syscall.h>
#include <utilities/tsc.h>

static int g_bcl_fd = -1;
static int g_pid = -1;

void fosBufferCacheLogReset(void)
{
    g_bcl_fd = -1;
}

static inline void fosBufferCacheInit()
{
    if(g_bcl_fd != -1) return;

    char *hare_build = getenv("HARE_BUILD");

    g_pid = getpid();

    char bcl_filename[1024];
    snprintf(bcl_filename, 1024, "%s/bcl/%s.%d", hare_build, "bcl", g_pid);

    g_bcl_fd = real_open(bcl_filename, O_RDWR | O_CREAT, 0777);
    fdAddExternal(g_bcl_fd);
}

void fosBufferCacheLog(int op, file_id_t id, uint64_t block)
{
    if(!BUFFER_CACHE_LOG) return;
    fosBufferCacheInit();

    char out[4096];
    char *op_str;
    switch(op)
    {
        case BCL_ACCESS: op_str = "ACCESS"; break;
        case BCL_DELETE: op_str = "DELETE"; break;
        default: op_str = "UNKNOWN"; break;
    }


    int len = snprintf(out, 4096, "%lld %s %d %lld %lld\n", rdtscll(), op_str, sched_getcpu(), id, block);

    int write_ret __attribute__((unused));
    write_ret = write(g_bcl_fd, out, len);

}
