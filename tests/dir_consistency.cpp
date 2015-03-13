#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(int ac, char **av)
{
    mkdir("d", 0777);

    int fd = creat("d/f", 0777);
    close(fd);

    pid_t child = fork();
    if(child == 0)
    {
        rename("d", "d2");
        exit(0);
    }

    usleep(50000);

    fd = open("d/f", O_RDWR);

    if(fd != -1)
    {
        close(fd);
        return -1;
    }

    unlink("d2/f");
    rmdir("d2");

    fd = creat("f", 0777);
    close(fd);

    child = fork();
    if(child == 0)
    {
        rename("f", "f2");
        exit(0);
    }

    usleep(50000);


    fd = open("f", O_RDWR);
    if(fd != -1)
    {
        close(fd);
        return -1;
    }

    unlink("f2");
    return 0;
}

