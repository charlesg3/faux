#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>

#define NUM_TRIES 1000000

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Usage: %s <ip>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(1024);

    saddr.sin_addr.s_addr = inet_addr(argv[1]);

    if(saddr.sin_addr.s_addr == INADDR_NONE)
    {
        printf("Invalid IP\n");
        return 2;
    }

    char *buf = "crj\n";
    char rbuf[4];

    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    // Send a few requests to get ARP stuff out of the way
    int i;
    for(i=0; i<3; i++)
    {
        sendto(fd, buf, 4, 0, (struct sockaddr*)&saddr, sizeof(saddr));
    }

    struct timeval begin;
    struct timeval end;
    gettimeofday(&begin, NULL);


    for(i=0; i<NUM_TRIES; i++)
    {
        sendto(fd, buf, 4, 0, (struct sockaddr*)&saddr, sizeof(saddr));
    }

    gettimeofday(&end, NULL);

    double elapsed = (end.tv_sec + end.tv_usec / 1000000.0) - 
                     (begin.tv_sec + begin.tv_usec / 1000000.0);

<<<<<<< HEAD
    fprintf(stderr, "Did %d requests in %f seconds (%f / sec)\n", NUM_TRIES, elapsed, NUM_TRIES/elapsed);





=======
    printf("Did %d requests in %f seconds (%f / sec)\n", NUM_TRIES, elapsed, NUM_TRIES/elapsed);

    return 0;
>>>>>>> ac790559d9923734285b030786ba77b3715395e4
}
