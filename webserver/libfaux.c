#include <stdio.h>


#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

int socket(int domain, int type, int protocol)
{
    printf("socket called.\n");
    return 0;
}

