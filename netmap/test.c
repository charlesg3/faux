#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/netmap.h>
#include <net/netmap_user.h>


#define E(format, ...)              \
      {fprintf(stderr, "%s [%d] " format "\n",     \
      __FUNCTION__, __LINE__, ##__VA_ARGS__); \
      exit(1);}



void debug_nmr(struct nmreq *nmr)
{
    
    fprintf(stderr,
            "Offset %p\n"
            "Size %dMiB\n"
            "TX Slots %d\n"
            "RX Slots %d\n"
            "TX Rings %d\n"
            "RX Rings %d\n"
            "Ring ID %d\n",
            nmr->nr_offset,
            nmr->nr_memsize>>20,
            nmr->nr_tx_slots,
            nmr->nr_rx_slots,
            nmr->nr_tx_rings,
            nmr->nr_rx_rings,
            nmr->nr_ringid);



}

int main(int argc, char **argv)
{

    if(argc < 2)
    {
        printf("Usage: %s <interface>\n", argv[0]);
        exit(1);
    }

    struct nmreq nmr;

    printf("Testing interface %s\n", argv[1]);

    int fd = open("/dev/netmap", O_RDWR);
    if(fd == -1)
        E("Failed to open netmap");

    bzero(&nmr, sizeof(nmr));
    nmr.nr_version = NETMAP_API;
    if((ioctl(fd, NIOCGINFO, &nmr)) == -1)
        E("Failed to get info");

    debug_nmr(&nmr);

    bzero(&nmr, sizeof(nmr));

    nmr.nr_version = NETMAP_API;
    strcpy(nmr.nr_name, argv[1]);
    int r = ioctl(fd, NIOCGINFO, &nmr);
    debug_nmr(&nmr);
    if(r == -1)
        E("Failed to get dev info for %s\n", nmr.nr_name);

    printf("Mapping %dMiB\n", nmr.nr_memsize>>20);

    struct netmap_d *mmap_addr = (struct netmap_d*) mmap(0, nmr.nr_memsize,
                                                PROT_WRITE | PROT_READ,
                                                MAP_SHARED, fd, 0);

    if(mmap_addr == MAP_FAILED)
        E("Failed to mmap file\n");

    bzero(&nmr, sizeof(nmr));
    nmr.nr_version = NETMAP_API;
    strcpy(nmr.nr_name, argv[1]);
    nmr.nr_ringid = 0;
    if (ioctl(fd, NIOCREGIF, &nmr) == -1)
        E("Failed to register interface\n");

    nmr.nr_rx_slots = 48;

    struct netmap_if *tif = NETMAP_IF(mmap_addr, nmr.nr_offset);
    struct netmap_ring *rxring = NETMAP_RXRING(tif, 0);

    //rxring->num_slots = 48;

    int total = 0;
    while(1)
    {
        if (ioctl(fd, NIOCRXSYNC, NULL) == -1)
            E("Failed to rx sync");


        printf("%d available of %d slots, %d processed\n", rxring->avail, rxring->num_slots, total+=rxring->avail);

        int i;
        for(i=0; i<rxring->avail; i++)
        {
            struct netmap_slot *slot = &rxring->slot[rxring->cur];
            char *p = NETMAP_BUF(rxring, slot->buf_idx);
            int len = slot->len;

            int j,k;
            int written = 0;
            printf("--------- GOT PACKET BYTES=%d ---------\n", slot->len); 
            for(j=0;;j++)
            {
                for(k=0; k<16; k++)
                {
                    printf("%02x ", (unsigned char)p[32*j+k]);
                    if(++written == slot->len)
                        goto donedebug;
                }
                printf("\n");
            }

donedebug:
            printf("\n\n");

            rxring->cur = NETMAP_RING_NEXT(rxring, rxring->cur);
        }

        rxring->avail = 0;

        usleep(100000);

    }

    return 0;
}

