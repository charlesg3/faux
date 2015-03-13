#include <lwip/tcp.h>
#include <lwip/dhcp.h>
#include <lwip/udp.h>
#include <lwip/dns.h>
#include <lwip/inet.h>
#include <lwip/init.h>
#include <lwip/err.h>

namespace fos { namespace net { namespace transport {
class Transport;
}}}

void ifUpCallback(struct netif *nf);
err_t loopif_init(struct netif *loopif);
err_t ethif_init(struct netif *loopif);
err_t loopif_output(struct netif *netif, struct pbuf *p,
        struct ip_addr *ipaddr);
err_t loopif_low_level_output(struct netif *netif, struct pbuf *p);
err_t eth_output(struct netif *netif, struct pbuf *p,
        struct ip_addr *ipaddr);
err_t ethif_low_level_output(struct netif *netif, struct pbuf *p);
struct netif * init_lwip(int id);
void init_lwip_set_transport(fos::net::transport::Transport * transport);
void lwip_idle();

