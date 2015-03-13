#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void term_handler (int sig)
{
    int ret = write(2, "term handler.\n", strlen("term handler.\n"));
    assert(ret != 0);
}

int main(int ac, char **av)
{
    int ret;

    int pipe_fd[2];
    ret = pipe2(pipe_fd, O_EXCL);
    assert(ret == 0);

    pid_t child = fork();

    if(child == 0)
    {
        char buf[1024];
        close(pipe_fd[1]);

        struct sigaction sa;
        bzero ((char *) &sa, sizeof sa);
        sa.sa_handler = term_handler;
        sa.sa_flags = 0;
        sigaction (SIGTERM, &sa, NULL);

        ret = read(pipe_fd[0], buf, 1024);
        assert(ret == -1);
        perror("read");
        exit(0);
    }

    close(pipe_fd[0]);

    usleep(20000);
    kill(child, SIGTERM);

    waitpid(child, NULL, 0);

    return 0;
}


