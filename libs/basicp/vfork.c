#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

pid_t vfork(void)
{
    return fork();
}
