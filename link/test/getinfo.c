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

    netif_info_t ninfo;
    netif_info(&ninfo, argv[1]);
    if(!ninfo.valid)
    {
        printf("Get info failed\n");
        return 1;
    }

    printf("Got info:\nTX Rings: %d\nTX Slots: %d\nRX Rings: %d\nRX Slots: %d\n",
            ninfo.tx_rings,
            ninfo.tx_slots,
            ninfo.rx_rings,
            ninfo.rx_slots);

    return 0;
}
