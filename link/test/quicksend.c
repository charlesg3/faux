#include <stdio.h>
#include <net/netif.h>
#include <sys/time.h>

#define NUM_PACKETS 100000

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
                 "55555555556666666666"
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666"
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666"
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666"
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666"
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666"
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666"
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666"
        "00000000001111111111222222222233333333334444444444"
                 "55555555556666666666"
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
        r = netif_quicksend(&nif, buf, 600);
        if(r < 0)
        {
            return 2;
        }
    }

    netif_poll(&nif, 0);

    gettimeofday(&end, NULL);

    double endval = end.tv_sec + ((double) end.tv_usec) / 1000000;
    double beginval = begin.tv_sec + ((double) begin.tv_usec) / 1000000;

    double diff = endval - beginval;

    printf("Wrote %d packets in %f seconds (%f pps)\n", NUM_PACKETS, diff, NUM_PACKETS/diff);

    netif_close(&nif);


    return 0;
}
