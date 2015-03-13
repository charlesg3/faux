#include <stdio.h>
#include <net/netif.h>
#include <sys/time.h>

#define NUM_PACKETS 1000000
#define BUFS_TO_GET 64

#define MIN(x,y) (x<y)?(x):(y)

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("You must specify an interface\n");
        return 1;
    }

    netif_info_t ninfo;
    netif_info(&ninfo, argv[1]);

    if(!ninfo.valid)
    {
        printf("Invalid interface\n");
        return 1;
    }

    if(ninfo.tx_rings == 1)
    {
        printf("Test skipped, only 1 tx ring\n");
        return 0;
    }

    netif_t nifs[ninfo.tx_rings];

    int i;
    for(i = 0; i<ninfo.tx_rings; i++)
    {
        int r;
        r = netif_init(&nifs[i], argv[1], i);
        if(r < 0)
        {
            printf("Open failed\nReturned %d\n", -r);
            return 1;
        }
    }


    char buf[] = 
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666";


    printf("Waiting\n");
    sleep(3);
    printf("Ready!\n");
 

    struct timeval begin;
    struct timeval end;

    gettimeofday(&begin, NULL);

    int packets_sent = 0;
    while(packets_sent < NUM_PACKETS)
    {
        int i;
        for(i=0; i<ninfo.tx_rings; i++)
        {
            int r;
            netif_packet_t packets[BUFS_TO_GET];
            r = netif_get_transmit_buffers(&nifs[i], packets, MIN(BUFS_TO_GET, NUM_PACKETS-packets_sent));
            if(r < 0)
            {
                printf("Failed to get bufs on ring %d\n", i);
                return 2;
            }
            //printf("Sending %d more on ring %d; %d/%d\n", r, i, packets_sent, NUM_PACKETS);
        
            int j;
            for(j = 0; j<r; j++)
            {
                memcpy(packets[j].data, buf, 60);
                *packets[j].len = 60;
            }

            packets_sent += r;
            r = netif_send_transmit_buffers(&nifs[i], r);
            if(r < 0)
            {
                return 3;
            }

            if(packets_sent == NUM_PACKETS)
                break;
        }

        //usleep(1);
    }

    gettimeofday(&end, NULL);

    double endval = end.tv_sec + ((double) end.tv_usec) / 1000000;
    double beginval = begin.tv_sec + ((double) begin.tv_usec) / 1000000;

    double diff = endval - beginval;

    printf("Wrote %d packets in %f seconds (%f pps)\n", NUM_PACKETS, diff, NUM_PACKETS/diff);

    int k;
    for(k=0; k<ninfo.tx_rings; k++)
        netif_close(&nifs[k]);


    return 0;
}
