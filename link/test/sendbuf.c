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

    char buf[] = 
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666";


    printf("Waiting\n");
    sleep(3);
    printf("Ready!\n");
 

    struct timeval begin;
    struct timeval end;

    gettimeofday(&begin, NULL);

    int packets_sent;
    while(packets_sent < NUM_PACKETS)
    {
        netif_packet_t packets[BUFS_TO_GET];
        r = netif_get_transmit_buffers(&nif, packets, MIN(BUFS_TO_GET, NUM_PACKETS-packets_sent));
        //printf("Sending %d more; %d/%d\n", r, packets_sent, NUM_PACKETS);
        if(r < 0)
        {
            return 2;
        }

        int i;
        for(i = 0; i<r; i++)
        {
            memcpy(packets[i].data, buf, 60);
            *packets[i].len = 60;
        }

        packets_sent += r;
        r = netif_send_transmit_buffers(&nif, r);
        if(r < 0)
        {
            return 3;
        }
    }

    gettimeofday(&end, NULL);

    double endval = end.tv_sec + ((double) end.tv_usec) / 1000000;
    double beginval = begin.tv_sec + ((double) begin.tv_usec) / 1000000;

    double diff = endval - beginval;

    printf("Wrote %d packets in %f seconds (%f pps)\n", NUM_PACKETS, diff, NUM_PACKETS/diff);

    netif_close(&nif);


    return 0;
}
