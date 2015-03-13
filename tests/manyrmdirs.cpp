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

#include <utilities/time_arith.h>

static int g_dir_count = 100;
static int g_file_count = 10;

void create_dirs(int id)
{
    int ret __attribute__((unused));
    for(int i = 0; i < g_dir_count; i++)
    {
        // create the directory
        char dirname[64];
        snprintf(dirname, 64, "d.%02d.%d", id, i);
        ret = mkdir(dirname, 0777);
        assert(ret == 0);

        // create files in the directory
        for(int j = 0; j < g_file_count; j++)
        {
            char filename[64];
            int fd;
            snprintf(filename, 64, "%s/p%02d.%d.%d", dirname, id, j, rand());
            fd = open(filename, O_RDWR | O_CREAT, 0777);
            if(fd < 0)
                fprintf(stderr, "couldn't create: %s\n", filename);
            assert(fd >= 0);
            close(fd);
        }
    }
}

void remove_dirs(int id)
{
    int ret __attribute__((unused));
    DIR *root = opendir(".");
    struct dirent *dent;
    bool cont = true;
    while((dent = readdir(root)) && cont)
    {
        if(strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
            continue;

        char myname[64];
        snprintf(myname, 64, "d.%02d", id);

        if(strncmp(dent->d_name, myname, strlen(myname)) != 0)
            continue;

        DIR *cur = opendir(dent->d_name);
        struct dirent *fent;
        while((fent = readdir(cur)))
        {
            if(strcmp(fent->d_name, ".") == 0 || strcmp(fent->d_name, "..") == 0)
                continue;
            char filename[64];
            snprintf(filename, 64, "%s/%s", dent->d_name, fent->d_name);
            ret = unlink(filename);
            assert(ret == 0);
        }
        closedir(cur);

        ret = rmdir(dent->d_name);
        assert(ret == 0);

        //FIXME: error with rmdir missing entries...
        closedir(root);
        root = opendir(".");
    }
    closedir(root);
}

void child_func(int id)
{
    create_dirs(id);
    remove_dirs(id);
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

    return 0;
}

