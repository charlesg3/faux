#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <list>
#include <vector>
#include <string>
#include <algorithm>

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdint.h>

#include <utilities/time_arith.h>

using namespace std;

void read_files(string dirname)
{
    int ret __attribute__((unused));
    DIR *dir = opendir(dirname.c_str());
    struct dirent64 *dent;

    vector<string> subdirs;

    while((dent = readdir64(dir)))
    {
        if(strcmp(dent->d_name, ".") == 0 ||
                strcmp(dent->d_name, "..") == 0)
            continue;
        if(dent->d_type == DT_DIR)
            subdirs.push_back(dent->d_name);
    }
    closedir(dir);

    std::random_shuffle(subdirs.begin(), subdirs.end());

    // now recurse into the subdirs
    for(auto i = subdirs.begin(); i != subdirs.end(); i++)
    {
        /*
            ret = chdir(i->c_str());
            assert(ret == 0);
            read_files(".");
            ret = chdir("..");
            assert(ret == 0);
        */
        string dir_s(dirname);
        dir_s.append("/").append(i->c_str());
        read_files(dir_s);
    }
}

void child_func(int id)
{
    read_files(".");
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

    uint64_t start_ts = rdtscll();

    if(ac == 1)
    {
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

    } else {
        int id = atoi(av[1]);
        srand(id);
        child_func(id);
        exit(0);
    }

    uint64_t end_ts = rdtscll();
    fprintf(stderr, "pfind sec: %.3f\n", ts_to_ms(end_ts - start_ts)/1000.0);
    return 0;
}


