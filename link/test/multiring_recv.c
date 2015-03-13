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

    if(info.rx_rings < 2)
    {
        printf("Skipping %s on %s; only %d rings\n", argv[0], argv[1], info.rx_rings);
        return 0;
    }

    netif_t nifs[info.rx_rings];
    int found_packets[info.rx_rings];

    int i;
    for(i=0; i<info.rx_rings; i++)
    {
        open_ring(&nifs[i], argv[1], i);
        found_packets[i] = 0;
    }




    printf("Waiting\n");
    sleep(3);
    printf("Ready!\n");




    for(i=0; i<NUM_TRIES; i++)
    {
        printf("Doing round %d\n", i);
        int j;
        for(j=0; j<info.rx_rings; j++)
        {
            int r = netif_poll(&nifs[j], 25);
            if(r < 0)
            {
                printf("Polling error on ring %d\n", j);
                return 3;
            }
            if(r > 0)
            {
                netif_packet_t bufs[r];
                netif_recv(&nifs[j], bufs, r);
                found_packets[j] += r;
            }
        }
    }
    
    
    int failure = 1;

    for(i=0;i<info.rx_rings; i++)
    {
        if(found_packets[i] != 0 || i == 0)
        {
            printf("Ring %d/%d got %d packets\n", i, info.rx_rings, found_packets[i]);
        }
        if(found_packets[i] != 0)
            failure = 0;

        netif_close(&nifs[i]);
    }

    if(failure)
    {
        printf("Failed to recv any packets!");
        return 4;
    }


    return 0;
}
