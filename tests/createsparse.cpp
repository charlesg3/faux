#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <string>

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdint.h>

#include <utilities/time_arith.h>

using namespace std;

static int g_topdirs = 1;
static int g_subdirs = 2;
static int g_sublevels = 14;

#define S_ISSHR S_ISVTX

void create_dirs(int sublevels, string path)
{
    char name[64];
    int ret __attribute__((unused));

    int dir_count = sublevels == g_sublevels ? g_topdirs : g_subdirs;
    for(int i = 0; i < dir_count; i++)
    {
        snprintf(name, 64, "d%d", i);
        string dirname(path + "/" + name);

        ret = mkdir(dirname.c_str(), 0777);
        assert(ret == 0);

        if(sublevels > 0) create_dirs(sublevels - 1, dirname);
    }
}

int main(int ac, char **av)
{
    uint64_t start_ts = rdtscll();
    create_dirs(g_sublevels, ".");
    uint64_t end_ts = rdtscll();

    fprintf(stderr, "create sec: %.2f\n", ts_to_ms(end_ts - start_ts)/1000.0);

    return 0;
}


