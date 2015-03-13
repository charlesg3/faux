#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdint.h>

#include <utilities/time_arith.h>

static int g_topdirs = 2;
static int g_subdirs = 10;
static int g_sublevels = 3;
static int g_files_per_level = 2000;
#define S_ISSHR S_ISVTX

void create_dirs(int sublevels)
{
    char name[64];
    int dir_count = sublevels == g_sublevels ? g_topdirs : g_subdirs;
    for(int i = 0; i < dir_count; i++)
    {
        snprintf(name, 64, "d%d", i);
        int ret __attribute__((unused));
        ret = mkdir(name, 0777 | S_ISSHR);
        assert(ret == 0);

        ret = chdir(name);
        assert(ret == 0);

        for(int j = 0; j < g_files_per_level; j++)
        {
            snprintf(name, 64, "f%d", j);
            int fd = open(name, O_WRONLY | O_CREAT, 0777);
            assert(fd >= 0);
            close(fd);
        }

        if(sublevels > 0) create_dirs(sublevels - 1);

        ret = chdir("..");
        assert(ret == 0);
    }
}

int main(int ac, char **av)
{
    uint64_t start_ts = rdtscll();
    create_dirs(g_sublevels);
    uint64_t end_ts = rdtscll();

    fprintf(stderr, "create sec: %.2f\n", ts_to_ms(end_ts - start_ts)/1000.0);

    return 0;
}


