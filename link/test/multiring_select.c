#include <stdio.h>
#include <net/netif.h>
#include <sys/time.h>
#include <stdlib.h>

#define NUM_TRIES 5

void open_ring(netif_t *nif, char *iface, int ring)
{
    int r = netif_init(nif, iface, ring);
    if(r < 0)
    {
        printf("Open ring %d failed\nReturned %d\n", ring, -r);
        exit(2);
    }
}

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("You must specify an interface\n");
        return 1;
    }

    netif_info_t info;
    netif_info(&info, argv[1]);
    if(!info.valid)
    {
        printf("Invalid interface %s\n", argv[1]);
        return 1;
    }

    netif_t nifs[info.rx_rings];
    int total_packets = 0;

    int i;
    for(i=0; i<info.rx_rings; i++)
    {
        open_ring(&nifs[i], argv[1], i);
    }




    printf("Waiting\n");
    sleep(3);
    printf("Ready!\n");




    for(i=0; i<NUM_TRIES; i++)
    {
        printf("Doing round %d\n", i);
        
        netif_t *readyifs[info.rx_rings];
        int readypackets[info.rx_rings];

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int r = netif_select(nifs, readyifs, readypackets, info.rx_rings, &tv);
        if(r < 0)
        {
            printf("Select error\n");
            return 3;
        }
        if(r > 0)
        {
            printf("Found packets on %d rings\n", r);

            int j;
            for(j=0; j<r; j++)
            {
                netif_packet_t bufs[readypackets[j]];
                netif_recv(readyifs[j], bufs, readypackets[j]);
                total_packets += readypackets[j];
                printf("Received %d packets on ring %d\n", readypackets[j], readyifs[j]->ring);
            }

            usleep(500000);
        }
    }
    
    printf("Received %d packets total\n", total_packets);
    
    if(total_packets == 0)
    {
        printf("Failed to recv any packets!\n");
        return 4;
    }


    return 0;
}
