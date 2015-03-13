#include <stdio.h>
#include <net/netif.h>
#include <sys/time.h>

#define POLL_TIMEOUT 1000
#define NUM_POLLS 5
#define DO_STATUS


void printpacket(const char *packet, size_t len)
{
    size_t i;

    printf("Packet [%u bytes]:\n", (unsigned int)len);
    for(i = 0; i<(len/16)+(len % 16 != 0)?(1):(0); i++)
    {
        int j;
        printf("0x%02x: ", (unsigned int)i*16); 
        for(j=0; j<16; j++)
        {
            if(16*i + j >= len)
                break;
            else
            {
                printf("%02x ", 0xFF&packet[16*i+j]);
            }
        }

        printf("\n");
    }

    printf("\n");
}

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

       if(r == 0)
           continue;

       netif_packet_t bufs[r];
       r = netif_recv(&nif, bufs, r);
       if(r < 0)
       {
           printf("Recv got error %d\n", r);
           return 3;
       }

#ifdef DO_STATUS
       int j;
       for(j=0; j<r; j++)
       {
           printpacket(bufs[j].data, *bufs[j].len); 
       }
#endif

       packets += r;

    }

    printf("Ring had a total of %d packets\n", packets);

    netif_close(&nif);

    if(packets == 0)
        return 4;


    return 0;
}
