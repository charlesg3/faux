#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <inttypes.h>

#include <utilities/time_arith.h>
#include <utilities/tsc.h>

#define S_ISSHR S_ISVTX
static int g_iterations = 16384*8;

void child_func(int id)
{
    char name[64];
    for(int i = 0; i < g_iterations; i++)
    {
        snprintf(name, 64, "d_p%d.%d.%d", id, i, rand());
        int ret __attribute__((unused));
        ret = mkdir(name, 0777);
        assert(ret == 0);
        ret = rmdir(name);
        assert(ret == 0);
    }
}

int main(int ac, char **av)
{
    char *cores_s = getenv("cpucount");
    if(!cores_s)
    {
        fprintf(stderr, "cpucount not specified on command line, using 1.\n");
        cores_s = "1";
    }
    int cores = atoi(cores_s);
    int children[cores];
    int ret __attribute__((unused));
    uint64_t start = rdtscll();

    if(ac == 1)
    {
        ret = mkdir("d", 01777 | S_ISSHR);
        ret = chdir("d");
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
        fprintf(stderr, "dirs: %.3f s\n", ts_to_ms(rdtscll() - start) / 1000.0);
        ret = chdir("..");
        ret = rmdir("d");

    } else {
        int id = atoi(av[1]);
        srand(id);
        child_func(id);
        exit(0);
    }

    return 0;
}

