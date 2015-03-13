#include <stdio.h>
#include <net/netif.h>
#include <sys/time.h>

#define POLL_TIMEOUT 1000
#define NUM_POLLS 5
#define DO_STATUS


int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("You must specify an interface\n");
        return 1;
    }

    netif_t nif;
    int r = netif_init(&nif, argv[1], 0);
    if(r < 0)
    {
        printf("Open failed\nReturned %d\n", -r);
        return 1;
    }

    r = netif_bind_cpu(&nif, 1, 0);
    if(r < 0)
    {
        printf("cpu bind failed\n");
        return -r;
    }

    printf("Waiting\n");
    sleep(3);
    printf("Ready!\n");
    
 
    int packets = 0; 
    int i;

    for(i=0; i<NUM_POLLS; i++)
    {
       int r = netif_poll(&nif, 1000);
       if(r < 0)
       {
           printf("Poll got error %d\n", r);
           return 2;
       }

#ifdef DO_STATUS
       printf("Have %d packets waiting in ring\n", r);
#endif

       packets = r;

    }

    printf("Ring had a total of %d packets\n", packets);

    netif_close(&nif);

    if(packets == 0)
    {
        printf("Failed to recv any packets!\n");
        return 4;
    }


    return 0;
}
