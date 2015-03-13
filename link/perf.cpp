#include <stdio.h>
#include <assert.h>
#include <arpa/inet.h>
#include <net/fauxlink.h>


#define ETHER_TYPE_IP 0x0800
#define ETHER_TYPE_ARP 0x0806
#define IP_VERSION 4
#define IP_HEADER_LEN 5;
#define IP_PROTO_UDP 17

#define ARP_HTYPE 0x01
#define ARP_PTYPE 0x0800
#define ARP_ETH_LEN 6
#define ARP_IP_LEN 4
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY 2


struct __attribute__((packed)) ether_frame
{
    char dest[6];
    char src[6];
    uint16_t type;
};

struct __attribute__((packed)) ip_frame
{
    struct ether_frame ether;
    char version:4;
    char headerlen:4;
    uint8_t tos;
    uint16_t len;
    uint16_t ident;
    uint16_t frags;
    uint8_t ttl;
    uint8_t proto;
    uint16_t checksum;
    char src[4];
    char dest[4];
};

struct __attribute__((packed)) arp_frame
{
    struct ether_frame ether;
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    char sender_mac[6];
    char sender_addr[4];
    char target_mac[6];
    char target_addr[4];
};

struct __attribute__((packed)) udp_frame
{
    struct ip_frame ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
};


void swap(void *t1, void *t2, size_t len)
{
    char temp[1024];
    if(len > 1024)
        len = 1024;
    memmove(temp, t2, len);
    memmove(t2, t1, len);
    memmove(t1, temp, len);
}


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



int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Usage %s <interface>\n", argv[0]);
        exit(1);
    }

    assert(sizeof(struct ether_frame) == 14);
    assert(sizeof(struct ip_frame) == sizeof(struct ether_frame) + 20);
    assert(sizeof(struct arp_frame) == sizeof(struct ether_frame) + 28);
    assert(sizeof(struct udp_frame) == sizeof(struct ip_frame) + 8);


    int rings = LocalNetworkLink::getRingCount(argv[1]);
    LocalNetworkLink *link = new LocalNetworkLink(argv[1], 0, rings);

    NetworkPacketList list;

    fprintf(stderr, "Receiving\n");

    while(true)
    {
        link->receive(&list, NULL);

        while(list.next())
        {
            char *buf;
            uint16_t len;
            list.get(&buf, &len);

            //fprintf(stderr, "Got packet len %d\n", len);

            if(len < 40)
                continue;


            struct ether_frame *ether = (struct ether_frame*)buf;
            if(ntohs(ether->type) == ETHER_TYPE_ARP)
            {
                //fprintf(stderr, "Is arp\n");
                struct arp_frame *arp = (struct arp_frame*)buf;
                if( ntohs(arp->htype) == ARP_HTYPE &&
                    ntohs(arp->ptype) == ARP_PTYPE &&
                    arp->hlen == ARP_ETH_LEN &&
                    arp->plen == ARP_IP_LEN &&
                    ntohs(arp->oper) == ARP_OP_REQUEST)
                {
                    
                    // ARP Request, send reply
                    
                    //fprintf(stderr, "Is arp request\n");
                    //fprintf(stderr, "Original:\n");
                    //printpacket(buf, len);

                    memcpy(&arp->ether.dest, &arp->ether.src, 6);
                    swap(&arp->target_addr, &arp->sender_addr, 4);
                    memcpy(&arp->ether.src, "\x00\x01\x11\x11\x11\x11", 6); 
                    memcpy(&arp->target_mac, &arp->sender_mac, 6);
                    memcpy(&arp->sender_mac, "\x00\x01\x11\x11\x11\x11", 6);

                    arp->oper = htons(ARP_OP_REPLY);
                    
                    //fprintf(stderr, "Sent:\n");
                    //printpacket(buf, len);

                    link->send(buf, len);
                }
            }
            else if(ntohs(ether->type) == ETHER_TYPE_IP)
            {
                //fprintf(stderr, "Is IP\n");
                struct ip_frame *ip = (struct ip_frame*)buf;
                if(ip->proto == IP_PROTO_UDP)
                {
                    struct udp_frame *udp = (struct udp_frame*)buf;

                    //fprintf(stderr, "Is UDP\n");
                    //fprintf(stderr, "Original:\n");
                    //printpacket(buf, len);

                    swap(&udp->ip.ether.src, &udp->ip.ether.dest, 6);
                    swap(&udp->ip.src, &udp->ip.dest, 4);
                    swap(&udp->src_port, &udp->dst_port, 2);

                    //fprintf(stderr, "Sent:\n");
                    //printpacket(buf, len);

                    link->send(buf, len);
                }
            }
        }
    }

}
