#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void udpRecv(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr * addr, u16_t port);
err_t connectionRecv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);
err_t connectionSent(void *arg, struct tcp_pcb *pcb, u16_t len);
void connectionError(void *arg, err_t err);
err_t connectionAccept(void *arg, struct tcp_pcb *tpcb, err_t err);
err_t connectionConnected(void *arg, struct tcp_pcb *tpcb, err_t err);
void connectionFinished(void *arg, struct tcp_pcb *tpcb);
void ifUpCallback(struct netif *nf);
err_t linkOutput(struct netif *netif, struct pbuf *p);

#ifdef __cplusplus
}
#endif
