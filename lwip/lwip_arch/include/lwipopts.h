#ifndef __LWIP_LWIPOPTS_H__
#define __LWIP_LWIPOPTS_H__
/*
 * Configuration for lwIP running on fos
 *
 * Author: Charles Gruenwald 
 */

#define NO_SYS 1
#define SYS_LIGHTWEIGHT_PROT 1
#define MEM_LIBC_MALLOC 1
//#define MEM_USE_POOLS 1
#define LWIP_TIMEVAL_PRIVATE 0
#define LWIP_DHCP 1
#define LWIP_DNS 1
#define LWIP_COMPAT_SOCKETS 0
#define LWIP_IGMP 1
#define LWIP_USE_HEAP_FROM_INTERRUPT 1
#define MEMP_NUM_SYS_TIMEOUT 10
#define MEMP_NUM_TCP_PCB             64 //number of concurrent connections essentially
#define MEMP_NUM_TCP_PCB_LISTEN      64 //number of concurrent connections essentially
#define MEM_SIZE                     67108864 //total amount of memory to use (64MB)
#define MEMP_NUM_PBUF                1024
#define MEMP_NUM_TCP_SEG             8192
#define TCP_SND_BUF 8194304
#define TCP_MSS 1460
#define TCP_WND (TCP_MSS * 40)
//#define LWIP_EVENT_API 1
#define LWIP_CALLBACK_API 1
#define LWIP_HAVE_LOOPIF 1
#define PBUF_POOL_SIZE                3200

//Don't use BSD Sockets
#if NO_SYS
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0
#define LWIP_NETIF_STATUS_CALLBACK 1
#endif

#endif /* __LWIP_LWIPOPTS_H__ */
