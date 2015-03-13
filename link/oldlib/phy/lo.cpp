#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <messaging/messaging.hpp>
#include <unistd.h>

#include <lwip/dhcp.h>
#include <lwip/tcp.h>
#include <netif/etharp.h>
#include <lwip/dns.h>
#include <lwip/inet.h>
#include <lwip/err.h>

#include "include/lo.hpp"

using namespace fos;
using namespace messaging;
using namespace net;
using namespace transport;
using namespace status;

Loopback *Loopback::m_loopback;

Loopback::Loopback()
{
    assert(m_loopback == NULL);
    m_loopback = this;
}

Loopback::~Loopback()
{
}

void Loopback::quit()
{
}

Status Loopback::send(void *vpdata, int len)
{
    // Forward directly to the link layer
    return netLinkIncoming(vpdata, len);
}

Status Loopback::sendV(struct iovec * in_iov, size_t in_iovcnt)
{
    return netLinkIncomingV(in_iov, in_iovcnt);
}

void Loopback::notifyStarted()
{
}

// C implementation

err_t loopif_init(struct netif *loopif)
{
    Loopback * lo_singleton = new Loopback();
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

err_t loopif_output(struct netif *netif, struct pbuf *p,
        struct ip_addr *ipaddr)
{
    return etharp_output(netif, p, ipaddr);
}


err_t loopif_low_level_output(struct netif *netif, struct pbuf *p)
{
#ifdef ETH_PAD_SIZE
    pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    if (!p->next) {
        Loopback::loopback().send(p->payload, p->len);
    } else {
        unsigned int pb_cnt;

        unsigned char data[p->tot_len], *cur;
        struct pbuf *q;

        for(q = p, pb_cnt = 0; q != NULL; ++pb_cnt, q = q->next);

        struct iovec iov[pb_cnt];
        unsigned int i;

        for(q = p, i = 0;  q != NULL; q = q->next, ++i)
        {
            iov[i].iov_base = q->payload;
            iov[i].iov_len = q->len;
        }

        Loopback::loopback().sendV(iov, pb_cnt);
    }

#if ETH_PAD_SIZE
    pbuf_header(p, ETH_PAD_SIZE);			/* reclaim the padding word */
#endif

    return ERR_OK;
}
