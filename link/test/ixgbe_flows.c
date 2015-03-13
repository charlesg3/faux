#include <stdio.h>
#include <sys/ioctl.h>
#include <net/netif.h>
#include <sys/time.h>
#include <errno.h>

#define RECV_LIMIT 5

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

    netif_info_t info;
    netif_info(&info, argv[1]);

    if(!info.valid)
    {
        printf("Invalid interface\n");
        return 1;
    }

    netif_t nifs[info.rx_rings];
    int i;
    for(i = 0; i<info.rx_rings; i++)
    {
        int r = netif_init(&nifs[i], argv[1], i);
        if(r < 0)
        {
            printf("Open failed\nReturned %d\n", -r);
            return 1;
        }
        

    }

    int r = netif_bind_cpu(nifs, info.rx_rings, sched_getcpu());
    if(r < 0)
    {
        printf("cpu bind failed\n");
        return -r;
    }

    r = netif_set_flow_groups(nifs, info.rx_rings/2, 0, 
            NETIF_NUM_FLOW_GROUPS/2, 2);
    r = netif_set_flow_groups(nifs+(info.rx_rings/2), info.rx_rings/2, 1, 
            NETIF_NUM_FLOW_GROUPS/2, 2);
    if(r != 0)
    {
        printf("add flow failed %d\n", r);
        errno = r;
        perror("err");
        return r;
    }
    

    netif_t *outifs[info.rx_rings];
    int outnums[info.rx_rings];

    int packetcount = 0;
    int tries = 0;
    while(packetcount < RECV_LIMIT && tries < 10)
    {
        tries++;
        int rings = 0;
        if(packetcount == 0)
            rings = netif_select(nifs, outifs, outnums, info.rx_rings, NULL);
        else
        {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 200000;
            rings = netif_select(nifs, outifs, outnums, info.rx_rings, &tv);
        }
        if(rings < 0)
        {
            printf("select failed %d\n", rings);
            perror("select");
            return rings;
        }
        
        int j;
        for(j=0; j<rings; j++)
        {
            netif_packet_t packets[outnums[j]];
            int rpnum = netif_recv(outifs[j], packets, outnums[j]);
            int k;
            for(k=0; k<rpnum; k++)
            {
                //printpacket(packets[k].data, *packets[k].len);
                if(packets[k].data[12] == 0x08 &&
                        packets[k].data[13] == 0x00)
                {
                    if(packets[k].data[9+14] == 0x06)
                    {
                        short source = ntohs(*(short*)&packets[k].data[9+14+11]);
                        short dest = ntohs(*(short*)&packets[k].data[9+14+11+2]);
                        printf("[%d] TCP source %d dest %d\n", packetcount+k, source, dest);
                        if((source % info.rx_rings) != outifs[j]->ring)
                        {
                            printf("ERROR: Found TCP packet from port %d on ring %d, " 
                                    "should be %d\n",
                                    source, outifs[j]->ring, source % info.rx_rings);
                            return 10;
                        }
                    }
                    else if(packets[k].data[9+14] == 0x11)
                    {
                        short source = ntohs(*(short*)&packets[k].data[9+14+11]);
                        short dest = ntohs(*(short*)&packets[k].data[9+14+11+2]);
                        printf("[%d] UDP source %d dest %d\n", packetcount+k, source, dest);
                        if((source % info.rx_rings) != outifs[j]->ring)
                        {
                            printf("ERROR: Found UDP packet from port %d on ring %d, " 
                                    "should be %d\n",
                                    source, outifs[j]->ring, source % info.rx_rings);
                            return 10;
                        }
                    }
                    else
                    {
                        printf("[%d] Unknown IP packet (%d bytes)\n", packetcount+k, *packets[k].len);
                    }
                }
            }
            printf("Found %d packets on ring %d\n", rpnum, outifs[j]->ring);
            packetcount += rpnum;
        }
    }


    for(i = 0; i<info.rx_rings; i++)
    {
        netif_close(&nifs[i]);
    }


    return 0;
}
