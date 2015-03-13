#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(int ac, char **av)
{
    int ret;

    pid_t child1 = fork();
    if(child1 == 0)
    {
        usleep(50000);
        ret = truncate("f", 0);
        int fd = creat("f2", 0777);
        for(int i = 0; i < 8; i++)
        {
            ret = write(fd, "c", 1);
            assert(ret == 1);
        }
        close(fd);
        exit(0);
        assert(ret == 0);
    }

    pid_t child2 = fork();
    if(child2 == 0)
    {
        int fd = creat("f", 0777);
        for(int i = 0; i < 8; i++)
        {
            ret = write(fd, "a", 1);
            assert(ret == 1);
        }
        usleep(150000);
        for(int i = 0; i < 8; i++)
        {
            ret = write(fd, "b", 1);
            assert(ret == 1);
        }
        close(fd);
        exit(0);
    }

    int status;
    child1 = wait(&status);
    child2 = wait(&status);

    struct stat buf;
    ret = stat("f", &buf);
    assert(ret == 0);

    fprintf(stderr, "size: %ld\n", buf.st_size);

    int fd = open("f", O_RDWR);

    for(int i = 0; i < buf.st_size; i++)
    {
        char c;
        ret = read(fd, &c, 1);
        assert(ret == 1);
        ret = putchar(c);
        assert(ret == c);
    }
    putchar('\n');
    close(fd);

    ret = stat("f2", &buf);
    assert(ret == 0);
    fprintf(stderr, "size: %ld\n", buf.st_size);
    fd = open("f2", O_RDWR);

    for(int i = 0; i < buf.st_size; i++)
    {
        char c;
        ret = read(fd, &c, 1);
        assert(ret == 1);
        ret = putchar(c);
        assert(ret == c);
    }
    putchar('\n');
    close(fd);


    ret = unlink("f");
    assert(ret == 0);
    ret = unlink("f2");
    assert(ret == 0);

    return 0;
}

