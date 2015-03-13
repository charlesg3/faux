#include <stdio.h>
#include <net/netif.h>
#include <sys/time.h>
#include <stdlib.h>

#define NUM_PACKETS 100000

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

    if(info.tx_rings < 5)
    {
        printf("Skipping %s on %s; only %d rings\n", argv[0], argv[1], info.tx_rings);
        return 0;
    }

    netif_t nif1;
    netif_t nif2;
    netif_t nif3;

    open_ring(&nif1, argv[1], 0);
    open_ring(&nif2, argv[1], 1);
    open_ring(&nif3, argv[1], 4);




    char buf[] = 
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666";


    printf("Waiting\n");
    sleep(3);
    printf("Ready!\n");
    
    
    struct timeval begin;
    struct timeval end;

    gettimeofday(&begin, NULL);

    int i;
    for(i = 0; i<NUM_PACKETS; i++)
    {
        int r1, r2;
        r1 = netif_send(&nif1, buf, 60);
        r2 = netif_send(&nif2, buf, 60);
        r2 = netif_send(&nif3, buf, 60);
        if(r1 < 0 || r2 < 0)
        {
            printf("A send failed, %d %d\n", r1, r2);
            return 2;
        }
    }

    gettimeofday(&end, NULL);

    double endval = end.tv_sec + ((double) end.tv_usec) / 1000000;
    double beginval = begin.tv_sec + ((double) begin.tv_usec) / 1000000;

    double diff = endval - beginval;

    printf("Wrote %d packets in %f seconds (%f pps)\n", NUM_PACKETS*3, diff, NUM_PACKETS*3.0/diff);

    netif_close(&nif1);
    netif_close(&nif2);


    return 0;
}
