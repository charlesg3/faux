/* 
 * lwip-net.c
 *
 * interface between lwIP's ethernet and Mini-os's netfront.
 * For now, support only one network interface, as mini-os does.
 *
 * Tim Deegan <Tim.Deegan@eu.citrix.net>, July 2007
 * based on lwIP's ethernetif.c skeleton file, copyrights as below.
 */


/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

//File edited for fos --Charles Gruenwald <cg3@csail.mit.edu>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"

#include <lwip/stats.h>
#include <lwip/sys.h>
#include <lwip/mem.h>
#include <lwip/memp.h>
#include <lwip/pbuf.h>
#include <netif/etharp.h>
#include <lwip/tcpip.h>
#include <lwip/tcp.h>
#include <lwip/netif.h>
#include <lwip/dhcp.h>

#include "netif/etharp.h"
#include <sys/net/types.h>


//FIXME: why is this defined here?
typedef signed short        s16;

#define ETH_HDR_SIZE 14

extern int g_id;

//#include <netfront.h>

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 'n'

#define IF_IPADDR	0x00000000
#define IF_NETMASK	0x00000000

/* Only have one network interface at a time. */
static struct netif *the_interface = NULL;

/* Forward declarations. */
static err_t netfront_output(struct netif *netif, struct pbuf *p,
        struct ip_addr *ipaddr);

/*
 * netfront_output():
 *
 * This function is called by the TCP/IP stack when an IP packet
 * should be sent. It calls the function called low_level_output() to
 * do the actual transmission of the packet.
 *
 */
static err_t
netfront_output(struct netif *netif, struct pbuf *p,
                struct ip_addr *ipaddr)
{

    /* resolve hardware address, then send (or queue) packet */
    return etharp_output(netif, p, ipaddr);
}

/* 
 * netif_rx(): gets called when a message is received from the netif
 * 
 * Pull received packets into a pbuf queue for the low_level_input() 
 * function to pass up to lwIP.
 */
struct tcp_pcb;
void netif_rx(unsigned char* data, int len, struct tcp_pcb ** ppcb)
{
    if (the_interface == NULL)
        return;
    struct netif *netif = the_interface;

    struct pbuf *p, *q;

#if ETH_PAD_SIZE
    len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

    /* move received packet into a new pbuf */
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (p == NULL) {
        printf("%s: No buffers for received packet\n", __func__);
        LINK_STATS_INC(link.memerr);
        LINK_STATS_INC(link.drop);
        return;
    }

#if ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

    /* We iterate over the pbuf chain until we have read the entire
     * packet into the pbuf. */
    for(q = p; q != NULL && len > 0; q = q->next) {
        /* Read enough bytes to fill this pbuf in the chain. The
         * available data in the pbuf is given by the q->len
         * variable. */
        memcpy(q->payload, data, len < q->len ? len : q->len);
        data += q->len;
        len -= q->len;
    }

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif

    LINK_STATS_INC(link.recv);

    struct ethernet_frame *eth_ptr = (struct ethernet_frame *)p->payload;
    /* volatile char *data2 = (char *)eth_ptr; */

    int type = ntohs(eth_ptr->type);
    if(type == 0x0800) //IP
    {
        etharp_ip_input(netif, p);
    }
    else if(type == 0x0806) //ARP
    {
        etharp_arp_input(netif, (struct eth_addr *) netif->hwaddr, p);
        return;
    }

    pbuf_header(p, -(s16)sizeof(struct eth_hdr));
    //printf("Netif_rx: Got a packet send to netif->input\n");
    netif->input(p, netif, ppcb);
    return;
}

/*
 * Set the IP, mask and gateway of the IF
 */
void networking_set_addr(struct ip_addr *ipaddr, struct ip_addr *netmask, struct ip_addr *gw)
{
    netif_set_ipaddr(the_interface, ipaddr);
    netif_set_netmask(the_interface, netmask);
    netif_set_gw(the_interface, gw);
}

/*
 * netif_netfront_init():
 *
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 */

err_t
netif_netfront_init(struct netif *netif)
{
    the_interface = netif;
    return ERR_OK;
}

/* 
 * Utility function to bring the whole lot up.  Call this from app_main() 
 * or similar -- it starts netfront and have lwIP start its thread,
 * which calls back to tcpip_bringup_finished(), which 
 * lets us know it's OK to continue.
 */
extern void tcpip_thread(void *unused);

struct netif *g_netif_en0;

void start_networking(char rawmac[6])
{
    struct netif *netif;
    struct ip_addr ipaddr = { htonl(IF_IPADDR) };
    struct ip_addr netmask = { htonl(IF_NETMASK) };
    struct ip_addr gw = { 0 };
    char *ip = "0.0.0.0";

    printf("Netstack using mac: %x:%x:%x:%x:%x:%x\n", \
            rawmac[0], rawmac[1], rawmac[2], \
            rawmac[3], rawmac[4], rawmac[5]);

    if (ip) {
        ipaddr.addr = inet_addr(ip);
        if (IN_CLASSA(ntohl(ipaddr.addr)))
            netmask.addr = htonl(IN_CLASSA_NET);
        else if (IN_CLASSB(ntohl(ipaddr.addr)))
            netmask.addr = htonl(IN_CLASSB_NET);
        else if (IN_CLASSC(ntohl(ipaddr.addr)))
            netmask.addr = htonl(IN_CLASSC_NET);
        else
            tprintk("Strange IP %s, leaving netmask to 0.\n", ip);
    }
    tprintk("IP %x netmask %x gateway %x.\n",
            ntohl(ipaddr.addr), ntohl(netmask.addr), ntohl(gw.addr));

    tprintk("TCP/IP bringup begins.\n");

    netif = malloc(sizeof(struct netif));
}

/* Shut down the network */
void stop_networking(void)
{
    /* fuck -cg3 */
}
