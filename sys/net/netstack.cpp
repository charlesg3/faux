#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include <messaging/dispatch.h>
#include <messaging/channel.h>
#include <name/name.h>

#include <config/config.h>

#include <core/core.h>
#include <net/fauxlink.h> //netmap stuff

#include <sys/net/faux_lwip.h>
#include <sys/net/link.h>
#include <sys/net/types.h>
#include <sys/net/transport.h>

#include "transport/transport_private.hpp"
#include "transport/transport_marshall.hpp"
#include "transport/transport_server.hpp"
#include "conn/conn_private.hpp"

#include <signal.h>

using namespace fos;
using namespace net;
using namespace conn;
using namespace transport;

NetworkLink *g_netlink;
NetworkLink *g_lo_netlink;

int g_id;
int g_transport_count;
int *g_child_procs;

#if CONFIG_UDP_ECHO
int g_udpechos = 0;
#endif

#define DEBUG_INPUT 0

void netmap_init(const char *if_name, unsigned int transport_count);
void linkserver_init(unsigned int id, unsigned int transport_count);
void idle(void *);
void check_input();
void netmap_cleanup();
void sig_handler(int signum);

void start_procs(const char *device, int transport_count);

bool g_use_linkserver = false;

int main(int argc, char **argv)
{
    int transport_count = argc == 3 ? atoi(argv[2]) : 1;
    const char *device ( argc < 2 ? "eth4" : argv[1] );
    fprintf(stderr, "Starting netstack...(children: %d) pid: %d\n", transport_count - 1, getpid());
    //sleep(5);

    g_transport_count = transport_count;
    g_use_linkserver = strcmp(device, "linkserver") == 0 ? true : false;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGHUP, sig_handler);

    start_procs(device, transport_count);

    return 0;
}

void start_procs(const char *device, int transport_count)
{
    pid_t childpid;
    int status;

    g_child_procs = (int *)malloc(sizeof(int) * transport_count);

    for(int i = 1; i < transport_count; i++)
    {
        childpid = fork();
        if ( childpid == -1 ) {
            perror("Cannot proceed. fork() error");
            exit(-1);
        }

        if (childpid == 0) // child
        {
            prctl(PR_SET_PDEATHSIG, SIGHUP);
            g_id = i;
            printf("Started netstack child: %d\n", i);
            fflush(stdout);
            goto finish_startup;
        }
        else
            g_child_procs[i] = childpid;
    }

    // Master init
    g_id = 0;

finish_startup:

#if CONFIG_USE_CORESERVER
    bind_to_default_core();
#endif

    name_init();
    if(g_use_linkserver)
        linkserver_init(g_id, transport_count);
    else
        netmap_init(device, transport_count);

    struct netif *nf = init_lwip(g_id);
    Transport *transport = new Transport(nf, g_id, transport_count);

    init_lwip_set_transport(transport);

    transport->go();

    netmap_cleanup();
    delete transport;
}

void linkserver_init(unsigned int id, unsigned int transport_count)
{
    g_netlink = new ServerNetworkLink(id, transport_count);
    g_lo_netlink = new LoopbackNetworkLink(id, transport_count);
}

void netmap_init(const char *if_name, unsigned int transport_count)
{
    int rings = LocalNetworkLink::getRingCount(if_name);
    if(rings == -1)
    {
        printf("Could not read ring count\n");
        exit(1);
    }
    int ring_block_size = rings / transport_count;
    int ring_start = g_id * ring_block_size;
    assert(ring_start + ring_block_size <= rings);

    g_netlink = new LocalNetworkLink(if_name, ring_start, ring_block_size);
    g_lo_netlink = new LoopbackNetworkLink(g_id, transport_count);

    LocalNetworkLink *llink = (LocalNetworkLink *)g_netlink;

    unsigned int total_flow_groups = llink->getTotalFlowGroupCount();
    unsigned int flow_group_block_size = total_flow_groups / transport_count;
    // Map flow_group_block_size flows starting at flow g_id with skip = transport_count
    // This will make sequential flow groups map to different transports
    llink->setFlowGroups(g_id, flow_group_block_size, transport_count);

    // Map DHCP to the first netstack
    if(g_id == 0)
        llink->setFlowGroups(67, 1, 1);
}

void netmap_cleanup()
{
    delete g_netlink;
    delete g_lo_netlink;
}


void idle(void * ignored)
{
    lwip_idle();
    check_input();
}

NetworkPacketList *g_packets = NULL;

static void swap(void *p1, void *p2, size_t size)
{
    uint8_t temp[size];
    memmove(temp, p1, size);
    memmove(p1, p2, size);
    memmove(p2, temp, size);
}

void process_input(char *data, int len)
{
    struct ethernet_frame *eth = (struct ethernet_frame *)data;
    int type = ntohs(eth->type);
    if(type == 0x0800) // ip
    {
        struct ip_frame *ip = (struct ip_frame *)&(((char *)data)[sizeof(struct ethernet_frame)]);
        /*
           printf("[%d] [%4d] bytes ", g_id, len);
           printf(" type: %02hhx%02hhx", ((char *)&eth->type)[0], ((char *)&eth->type)[1]);
           printf(" prot: %d", ip->protocol);
           printf("from: %hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           eth->source[0], eth->source[1], eth->source[2], eth->source[3], eth->source[4], eth->source[5]);
           printf(" to: %hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           eth->dest[0], eth->dest[1], eth->dest[2], eth->dest[3], eth->dest[4], eth->dest[5]);
           */

        if(ip->protocol == 6) // tcp
        {
            unsigned int header_len = ip->version & 0xF;
            struct tcp_frame *tf = (struct tcp_frame *) &data[sizeof(struct ethernet_frame) + (header_len * 4)];

            in_addr_t source = ip->source;
            in_addr_t dest = ip->dest;

            uint8_t flags = ((uint8_t *) &tf->control)[1];

            if((flags & TCP_SYN) && !(flags & TCP_ACK)) // SYN packet
            {
                if(g_id != 0 && DEBUG_INPUT)
                    fprintf(stderr, "[%d] new connection src: %d dest: %d.\n", 
                            g_id,
                            ntohs(tf->source_port),
                            ntohs(tf->dest_port));
                //fprintf(stderr, "incoming conn detected...\n");
                ConnLink::connLink().incomingConn(
                        source,
                        ntohs(tf->source_port),
                        dest,
                        ntohs(tf->dest_port),
                        data,
                        len);
                return;
            }
        }
        else if(ip->protocol == 17)
        {
#if CONFIG_UDP_ECHO
            unsigned int header_length = ip->version & 0xF;
            struct udp_frame *udp = (struct udp_frame *)&((char *)data)[sizeof(struct ethernet_frame) + (header_length * 4)];


            if(ntohs(udp->dest_port) == 1024)
            {
                swap(&eth->source, &eth->dest, 6);
                swap(&ip->source, &ip->dest, 4);
                swap(&udp->source_port, &udp->dest_port, 2);

                g_netlink->send(data, len);

                g_udpechos += 1;
                return;
            }
#endif
        }
    }
    //fflush(stdout);
    else if(type == 0x0806)
    {
        assert(g_id == 0);
        struct arp_frame *ap = (struct arp_frame *)&(((char *)data)[sizeof(struct ethernet_frame)]);
        uint8_t oper = ntohs(ap->oper);
        if(oper == 2) // ARP Reply -- broadcast to child netstacks
            Transport::transport().incomingBroadcast((unsigned char *)data, len);
    }
    Transport::transport().incoming((unsigned char *)data, len);
}

void check_input()
{
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if(!g_packets)
        g_packets = new NetworkPacketList();

    int r;
#if 0
    r = g_netlink->poll();
    if(r == 0)
    {
        g_netlink->tick();
        return;
    }
#endif 

    char *data;
    uint16_t len;

    r = g_netlink->receive(g_packets, &timeout);
    if(r < 0)
    {
        printf("Receive got error %d\n", r);
        return;
    }
    else if(r > 0)
    {
        while(g_packets->next())
        {
            g_packets->get(&data, &len);
            process_input(data, len);
        }
    }

    r = g_lo_netlink->receive(g_packets, &timeout);
    if(r < 0)
    {
        printf("Receive got error %d\n", r);
        return;
    }
    else if(r > 0)
    {
        while(g_packets->next())
        {
            g_packets->get(&data, &len);
            process_input(data, len);
        }
    }


}


void sig_handler(int signum)
{
#if CONFIG_UDP_ECHO
    fprintf(stderr, "UDP Echo count: %d\n", g_udpechos);
#endif

    if(g_id == 0)
    {
        for(int i = 1; i < g_transport_count; i++)
        {
            int status;
            kill(g_child_procs[i], SIGINT);
            pid_t done = wait(&status);
        }

    }

    dispatchStop();

    if(g_id == 0)
        free(g_child_procs);
}
