#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include <utilities/time_arith.h>
#include <netif/etharp.h>
#include <inttypes.h>
#include <sys/net/link.h>
#include <sys/net/types.h>
#include <sys/net/faux_lwip.h>

#include <net/fauxlink.h> //netmap stuff
#include <transport_server.hpp>  //to call the ifUpCallback on the transport server

#include <config/config.h>

extern "C" void netif_netfront_init(struct netif *netif);

extern NetworkLink *g_netlink;
extern NetworkLink *g_lo_netlink;
extern int g_id;

static struct netif *g_netif = NULL;
static struct netif *g_loopif = NULL;
static fos::net::transport::Transport * g_transport = NULL;

static struct timeval g_last_tcp_time, g_last_dns_time, g_last_dhcp_fine_time, g_last_dhcp_coarse_time, g_last_arp_time;

void ifUpCallback(struct netif *nf)
{
    g_transport->ifUpCallback(ip_to_in(nf->ip_addr), ip_to_in(nf->netmask), ip_to_in(nf->gw));
}

err_t loopif_init(struct netif *loopif)
{
    struct ip_addr lo_ipaddr, lo_netmask, lo_gw;
    //add loop interface //set local loop-interface 127.0.0.1
    IP4_ADDR(&lo_gw, 127,0,0,1);
    IP4_ADDR(&lo_ipaddr, 127,0,0,1);
    IP4_ADDR(&lo_netmask, 255,0,0,0);
    netif_set_ipaddr(loopif, &lo_ipaddr);
    netif_set_netmask(loopif, &lo_netmask);
    netif_set_gw(loopif, &lo_gw);
    loopif->name[0] = 'l';
    loopif->name[1] = 'o';
    loopif->num = 1;
    loopif->output = loopif_output;
    loopif->linkoutput = loopif_low_level_output;
    loopif->hwaddr_len = 6;
    loopif->state = NULL;

    struct eth_addr ethaddr;
    memcpy(ethaddr.addr, loopif->hwaddr, sizeof(ethaddr.addr));
    update_arp_entry(loopif, &lo_ipaddr, &ethaddr, ETHARP_TRY_HARD);

    netif_set_up(loopif);
    return ERR_OK;
}

err_t ethif_init(struct netif *netif)
{
#if LWIP_SNMP
    /* ifType ethernetCsmacd(6) @see RFC1213 */
    netif->link_type = 6;
    /* your link speed here */
    netif->link_speed = ;
    netif->ts = 0;
    netif->ifinoctets = 0;
    netif->ifinucastpkts = 0;
    netif->ifinnucastpkts = 0;
    netif->ifindiscards = 0;
    netif->ifoutoctets = 0;
    netif->ifoutucastpkts = 0;
    netif->ifoutnucastpkts = 0;
    netif->ifoutdiscards = 0;
#endif

    netif->name[0] = 'e';
    netif->name[1] = 'n';
    netif->output = eth_output;
    netif->linkoutput = ethif_low_level_output;

    /* set MAC hardware address */
    netif->hwaddr_len = 6;
    g_netlink->getmac(netif->hwaddr);

    /* No interesting per-interface state */
    netif->state = NULL;

    /* maximum transfer unit */
    netif->mtu = 1500;

    /* broadcast capability */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    //  etharp_init();

    netif_netfront_init(netif);

    return ERR_OK;
}

err_t eth_output(struct netif *netif, struct pbuf *p,
        struct ip_addr *ipaddr)
{
    return etharp_output(netif, p, ipaddr);
}

err_t loopif_output(struct netif *netif, struct pbuf *p,
        struct ip_addr *ipaddr)
{
    return etharp_output(netif, p, ipaddr);
}

err_t loopif_low_level_output(struct netif *netif, struct pbuf *p)
{
    if (!p->next)
    {
        int r = g_lo_netlink->send((char*)p->payload, p->len);
        if(r < 0) fprintf(stderr, "Error sending!\n");
    } else {
        fprintf(stderr, "unhandled multi-length packet.\n");
    }

    return ERR_OK;
}

err_t ethif_low_level_output(struct netif *netif, struct pbuf *p)
{
#ifdef ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

    /*
       char *in_data = (char *)p->payload;

       printf("[%4d] bytes out ", p->len);

       struct ethernet_frame *eth = (struct ethernet_frame *)in_data;
       printf("from: %2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
       eth->source[0],
       eth->source[1],
       eth->source[2],
       eth->source[3],
       eth->source[4],
       eth->source[5]);
       printf(" to: %2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
       eth->dest[0],
       eth->dest[1],
       eth->dest[2],
       eth->dest[3],
       eth->dest[4],
       eth->dest[5]);

       printf(" type: %2hhx%2hhx", ((char *)&eth->type)[0], ((char *)&eth->type)[1]);

       struct ip_frame *ip = (struct ip_frame *)&(((char *)in_data)[sizeof(struct ethernet_frame)]);
       printf(" ip protocol: %d\n", ip->protocol);


       if(ip->protocol == 17)
       {
       unsigned int header_length = ip->version & 0xF;
       struct udp_frame *udp = (struct udp_frame *)&((char *)in_data)[sizeof(struct ethernet_frame) + (header_length * 4)];
       printf(" src port: %d dest port: %d\n", ntohs(udp->source_port), ntohs(udp->dest_port));
       }
       fflush(stdout);
       */

    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    if (!p->next)
    {
        int r = g_netlink->send((char*)p->payload, p->len);
        if(r < 0) fprintf(stderr, "Error sending!\n");
    } else {
        fprintf(stderr, "unhandled multi-length packet.\n");
    }

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE);/* reclaim the padding word */
#endif

    return ERR_OK;
}

bool start_echo = true;
bool started_echo = false;

bool initted_ip = false;

void lwip_idle()
{
    // Set the static ip and propagate it
#ifdef CONFIG_STATIC_IP
    if(g_id == 0 && !initted_ip)
    {
        initted_ip = true;
        in_addr_t ip_addr, netmask, gw;
        inet_pton(AF_INET, CONFIG_STATIC_IP_ADDRESS, &ip_addr);
        inet_pton(AF_INET, CONFIG_STATIC_NETMASK, &netmask);
        inet_pton(AF_INET, CONFIG_STATIC_GATEWAY, &gw);
        g_transport->ifUpCallback(ip_addr, netmask, gw);
    }
#endif

    struct timeval now;
    gettimeofday(&now, NULL);

    // Make sure we have an ip address first
    if(start_echo && !started_echo && g_transport->getIP())
    {
        started_echo = true;
        g_transport->startEcho(4321);
    }

    if(timeval_diff_ms(&now, &g_last_tcp_time) > TCP_TMR_INTERVAL)
    {
        tcp_tmr();
        g_last_tcp_time = now;
    }

    if(timeval_diff_ms(&now, &g_last_dns_time) > DNS_TMR_INTERVAL)
    {
        dns_tmr();
        g_last_dns_time = now;
    }

    if(timeval_diff_ms(&now, &g_last_dhcp_fine_time) > DHCP_FINE_TIMER_MSECS)
    {
        dhcp_fine_tmr();
        g_last_dhcp_fine_time = now;
    }

    if(timeval_diff_ms(&now, &g_last_dhcp_coarse_time) > DHCP_COARSE_TIMER_MSECS)
    {
        dhcp_coarse_tmr();
        g_last_dhcp_coarse_time = now;
    }

    if(timeval_diff_ms(&now, &g_last_arp_time) > ARP_TMR_INTERVAL)
    {
        etharp_tmr();
        g_last_arp_time = now;
    }

}

void init_lwip_set_transport(fos::net::transport::Transport * transport)
{
    g_transport = transport;
}


struct netif * init_lwip(int id)
{
    MacAddress *mac = (MacAddress *)malloc(sizeof(mac)); //FIXME: where does this get freed? --cg3

    struct ip_addr ipaddr = { htonl(0x00000000) };
    struct ip_addr netmask = { htonl(0x00000000) };
    struct ip_addr gw = { 0 };

    g_netif = (struct netif *)malloc(sizeof(struct netif));
    g_loopif = (struct netif *)malloc(sizeof(struct netif));
    memset(g_loopif, 0, sizeof(struct netif));

    lwip_init(LWIP_START_DNS);

    struct ip_addr lo_ipaddr, lo_netmask, lo_gw;
    netif_add(g_loopif, &lo_ipaddr, &lo_netmask, &lo_gw, NULL, loopif_init,
            ip_input);

    netif_add(g_netif, &ipaddr, &netmask, &gw, mac, ethif_init, ip_input);

    g_netif->state = mac;
    g_netif->linkoutput = ethif_low_level_output;
    netif_set_default(g_netif);

#ifndef CONFIG_STATIC_IP
    if(id == 0) // only the master netstack starts dhcp
    {
        netif_set_status_callback(g_netif, ifUpCallback);
        dhcp_start(g_netif);
    }
#endif

    etharp_init();

    memset(&g_last_tcp_time, 0, sizeof(g_last_tcp_time));
    memset(&g_last_dns_time, 0, sizeof(g_last_dns_time));
    memset(&g_last_dhcp_fine_time, 0, sizeof(g_last_dhcp_fine_time));
    memset(&g_last_dhcp_coarse_time, 0, sizeof(g_last_dhcp_coarse_time));
    memset(&g_last_arp_time, 0, sizeof(g_last_arp_time));
    return g_netif;
}
