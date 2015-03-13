#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sched.h>

#include <utilities/time_arith.h>

static int g_cores;
static int g_iterations = 4096;
static int g_write_size = 4096;
static int g_writes = 10000;

static void create_file(char *filename);
static void copy_file(char *in_s, char *out_s);
static void remove_file(char *filename);
//static int g_socket_map[40] = {0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3};

static void copy_file(char *in_s, char *out_s)
{
    int in_fd = open(in_s, O_RDONLY);
    int out_fd = open(out_s, O_RDWR | O_CREAT, 0777);
    char buf[16384];
    int readsize;

    while((readsize = read(in_fd, buf, sizeof(buf))))
    {
        int ret __attribute__((unused));
        ret = write(out_fd, buf, readsize);
        assert(ret == readsize);
    }

    close(in_fd);
    close(out_fd);
}

void child_func(int id)
{
    char dest[64];
    for(int i = 0; i < g_iterations; i++)
    {
        char src[64];
        snprintf(src, sizeof(src), "f%d", rand() % g_cores);

        snprintf(dest, 64, "p%d.%d.%d", id, i, rand());
        copy_file(src, dest);
        remove_file(dest);
    }
}

static void create_file(char *filename)
{
    char buf[g_write_size];
    int fd = open(filename, O_WRONLY | O_CREAT, 0777);

    for(int i = 0; i < g_writes; i++)
    {
        if(write(fd, buf, g_write_size) < 0)
            assert(false);
    }

    close(fd);
}

static void remove_file(char *filename)
{
    int ret __attribute__((unused)) = unlink(filename);
    assert(ret == 0);
}

int main(int ac, char **av)
{
    char *cores_s = getenv("cpucount");
    if(!cores_s)
    {
        fprintf(stderr, "cpucount not specified on command line, using 1.\n");
        cores_s = "1";
    }
    g_cores = atoi(cores_s);

    char *filesize_s = getenv("FILESIZE");
    if(!filesize_s)
    {
        filesize_s = "128";
        fprintf(stderr, "filesize not specified on command line, using %s.\n", filesize_s);
    }
    int filesize_k = atoi(filesize_s);
    int filesize_b = 1024 * filesize_k;
    assert(filesize_b >= g_write_size);
    g_writes = filesize_b / g_write_size;

    for(int i = 0; i < g_cores; i++)
    {
        char fname[128];
        snprintf(fname, sizeof(fname), "f%d", i);
        create_file(fname);
    }

    int children[g_cores];

    for(int i = 0; i < g_cores; i++)
    {
        pid_t child = fork();
        if(child == 0)
        {
            srand(i);
            child_func(i);
            exit(0);
        }
        else
            children[i] = child;
    }

    for(int i = 0; i < g_cores; i++)
        waitpid(children[i], NULL, 0);

    for(int i = 0; i < g_cores; i++)
    {
        char fname[128];
        snprintf(fname, sizeof(fname), "f%d", i);
        remove_file(fname);
    }

    return 0;
}

