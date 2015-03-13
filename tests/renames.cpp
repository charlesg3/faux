#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sched.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <inttypes.h>

#include <utilities/time_arith.h>
#include <utilities/tsc.h>

static int g_iterations = 16384*32;
static int g_dir_count = 10;

void child_func(int id)
{
    int ret __attribute__((unused));
    char src[64];
    char dest[64];

    // create a directory to be moved
    snprintf(dest, 64, "p%d_d", id);
    int fd = open(dest, O_RDWR | O_CREAT, 0777);
    close(fd);
//    ret = mkdir(dest, 0777);
//    assert(ret == 0);

//    uint64_t nct = 0;
//    uint64_t rt = 0;

    for(int i = 0; i < g_iterations; i++)
    {
        //uint64_t start = rdtscll();
        snprintf(src, 64, "%s", dest);
        snprintf(dest, 64, "d%d/%d.%d.%d", rand() % g_dir_count, id, i % g_dir_count, rand());
        /*
        uint64_t mid = rdtscll();
        if(mid - start > 0)
            nct += (mid - start);
            */

        ret = rename(src, dest);
        /*
        uint64_t end = rdtscll();
        if(end - mid > 0)
            rt += (end - mid);
            */
        assert(ret == 0);
    }

//    fprintf(stderr, "[%d] nct: %.3f s\n", sched_getcpu(), ts_to_ms(nct) / 1000.0);
//    fprintf(stderr, "[%d] rt: %.3f s\n", sched_getcpu(), ts_to_ms(rt) / 1000.0);

//    ret = rmdir(dest);
    ret = unlink(dest);
    assert(ret == 0);
}

int main(int ac, char **av)
{
    int ret;
    char *cores_s = getenv("cpucount");
    if(!cores_s)
    {
        fprintf(stderr, "cpucount not specified on command line, using 1.\n");
        cores_s = "1";
    }

    int cores = atoi(cores_s);
    int children[cores];
    uint64_t start = rdtscll();

    if(ac == 1)
    {
        char name[64];

        for(int i = 0; i < g_dir_count; i++)
        {
            snprintf(name, 64, "d%d", i);
            ret = mkdir(name, 01777);
            assert(ret == 0);
        }

        if(cores == 1)
        {
            child_func(0);
        }
        else
        {
            for(int i = 0; i < cores; i++)
            {
                pid_t child = fork();
                if(child == 0)
                {
                    char *args[3];
                    char id_str[64];
                    snprintf(id_str, 64, "%d", i);
                    args[0] = av[0];
                    args[1] = id_str;
                    args[2] = 0;
                    execv(av[0], args);
                }
                else
                    children[i] = child;
            }

            for(int i = 0; i < cores; i++)
                waitpid(children[i], NULL, 0);

        }
        fprintf(stderr, "renaming: %.3f s\n", ts_to_ms(rdtscll() - start) / 1000.0);
        for(int i = 0; i < g_dir_count; i++)
        {
            snprintf(name, 64, "d%d", i);
            ret = rmdir(name);
            assert(ret == 0);
        }

    } else {
        int id = atoi(av[1]);
        srand(id);
        child_func(id);
        exit(0);
    }



    return 0;
}

