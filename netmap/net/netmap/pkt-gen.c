/*
 * Copyright (C) 2011 Matteo Landi, Luigi Rizzo. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $FreeBSD: head/tools/tools/netmap/pkt-gen.c 227614 2011-11-17 12:17:39Z luigi $
 * $Id: pkt-gen.c 10639 2012-02-24 16:40:10Z luigi $
 *
 * Example program to show how to build a multithreaded packet
 * source/sink using the netmap device.
 *
 * In this example we create a programmable number of threads
 * to take care of all the queues of the interface used to
 * send or receive traffic.
 *
 */

const char *default_payload="netmap pkt-gen Luigi Rizzo and Matteo Landi\n"
	"http://info.iet.unipi.it/~luigi/netmap/ ";

#include <errno.h>
#include <pthread.h>	/* pthread_* */
#include <signal.h>	/* signal */
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>	/* PRI* macros */
#include <string.h>	/* strcmp */
#include <fcntl.h>	/* open */
#include <unistd.h>	/* close */
#include <ifaddrs.h>	/* getifaddrs */

#include <sys/mman.h>	/* PROT_* */
#include <sys/ioctl.h>	/* ioctl */
#include <sys/poll.h>
#include <sys/socket.h>	/* sockaddr.. */
#include <arpa/inet.h>	/* ntohs */
#include <sys/param.h>
#include <sys/sysctl.h>	/* sysctl */
#include <sys/time.h>	/* timersub */

#include <net/ethernet.h>
#include <net/if.h>	/* ifreq */

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <net/netmap.h>
#include <net/netmap_user.h>
#include <pcap/pcap.h>

#ifdef __FreeBSD__
#include <pthread_np.h>	/* pthread w/ affinity */
#include <sys/cpuset.h>	/* cpu_set */
#include <net/if_dl.h>	/* LLADDR */
#else /* FreeBSD */
#include <netinet/ether.h>	/* ether_aton */
#include <linux/if_packet.h>	/* sockaddr_ll */
#define __unused __attribute__((__unused__))
#endif /* !FreeBSD */

static inline int min(int a, int b) { return a < b ? a : b; }

/* debug support */
#define D(format, ...)				\
	fprintf(stderr, "%s [%d] " format "\n", 	\
	__FUNCTION__, __LINE__, ##__VA_ARGS__)

int verbose = 0;
#define MAX_QUEUES 64	/* no need to limit */

#define SKIP_PAYLOAD 1 /* do not check payload. */


struct pkt {
	struct ether_header eh;
	struct ip ip;
	struct udphdr udp;
	uint8_t body[0];
} __attribute__((__packed__));

/*
 * global arguments for all threads
 */
struct glob_arg {
	const char *src_ip;
	const char *dst_ip;
	const char *src_mac;
	const char *dst_mac;
	int pkt_size;
	int burst;
	int npackets;	/* total packets to send */
	int nthreads;
	int cpus;
	int use_pcap;
	int copy_all;
	pcap_t *p;
};

struct mystat {
	uint64_t containers[8];
};

/*
 * Arguments for a new thread. The same structure is used by
 * the source and the sink
 */
struct targ {
	struct glob_arg *g;
	int used;
	int completed;
	int fd;
	struct nmreq nmr;
	struct netmap_if *nifp;
	uint16_t	qfirst, qlast; /* range of queues to scan */
	uint64_t count;
	struct timeval tic, toc;
	int me;
	int ring_pkts;
	int pkt_size;
	pthread_t thread;
	int affinity;

	uint8_t	dst_mac[6];
	uint8_t	src_mac[6];
	u_int dst_mac_range;
	u_int src_mac_range;
	uint32_t dst_ip;
	uint32_t src_ip;

	struct pkt **pkts;
};


static struct targ *targs;
static int global_nthreads;

/* control-C handler */
static void
sigint_h(__unused int sig)
{
	int i;

	for (i = 0; i < global_nthreads; i++) {
		/* cancel active threads. */
		if (targs[i].used == 0)
			continue;

		D("Cancelling thread #%d\n", i);
		pthread_cancel(targs[i].thread);
		targs[i].used = 0;
	}

	signal(SIGINT, SIG_DFL);
}

#ifdef __FreeBSD__
/* sysctl wrapper to return the number of active CPUs */
static int
system_ncpus(void)
{
	int mib[2], ncpus;
	size_t len;

	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof(mib);
	sysctl(mib, 2, &ncpus, &len, NULL, 0);

	return (ncpus);
}
#endif

/*
 * locate the src mac address for our interface, put it
 * into the user-supplied buffer. return 0 if ok, -1 on error.
 */
#ifdef __linux__
#define sockaddr_dl	sockaddr_ll
#define sdl_family	sll_family
#define AF_LINK	AF_PACKET
#define LLADDR(s)	s->sll_addr;
#endif /* linux */
static int
source_hwaddr(const char *ifname, char *buf)
{
	struct ifaddrs *ifaphead, *ifap;
	int l = sizeof(ifap->ifa_name);

	if (getifaddrs(&ifaphead) != 0) {
		D("getifaddrs %s failed", ifname);
		return (-1);
	}

	for (ifap = ifaphead; ifap; ifap = ifap->ifa_next) {
		struct sockaddr_dl *sdl =
			(struct sockaddr_dl *)ifap->ifa_addr;
		uint8_t *mac;

		if (!sdl || sdl->sdl_family != AF_LINK)
			continue;
		if (strncmp(ifap->ifa_name, ifname, l) != 0)
			continue;
		mac = (uint8_t *)LLADDR(sdl);
		sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
			mac[0], mac[1], mac[2],
			mac[3], mac[4], mac[5]);
		if (verbose)
			D("source hwaddr %s", buf);
		break;
	}
	freeifaddrs(ifaphead);
	return ifap ? 0 : 1;
}

/* set the thread affinity. */
static int
setaffinity(pthread_t me, int i)
{
#ifdef __FreeBSD__
	cpuset_t cpumask;

	if (i == -1)
		return 0;

	/* Set thread affinity. */
	if (bind_to_core(i) != 0) {
		D("Unable to set affinity");
		return 1;
	}
#endif /* FreeBSD */
	return 0;
}


static uint16_t
checksum(const void *data, uint16_t len, uint32_t sum)
{
        const uint8_t *addr = data;
	uint32_t i;

	/* Checksum all the pairs of bytes first... */
	for (i = 0; i < (len & ~1U); i += 2) {
		sum += (u_int16_t)ntohs(*((u_int16_t *)(addr + i)));
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}

	/*
	 * If there's a single byte left over, checksum it, too.
	 * Network byte order is big-endian, so the remaining byte is
	 * the high byte.
	 */
	if (i < len) {
		sum += addr[i] << 8;
		if (sum > 0xFFFF)
			sum -= 0xFFFF;
	}

	return (sum);
}

static u_int16_t
wrapsum(u_int32_t sum)
{
	sum = ~sum & 0xFFFF;
	return (htons(sum));
}

static int
address_range(char *address)
{
	char *p;
	int range = 1;

	p = index(address, '%');
	if (p) {
		range = MIN(256, MAX(1, atoi(p + 1)));
		/* To make the inet_aton pass, we need to replace the %n stuff
		 * with a 0 and a character of eos.
		 */
		p[0] = '0';
		p[1] = '\0';
	}

	return range;
}
/*
 * Fill a packet with some payload.
 */
#ifdef __linux__
#define uh_sport source
#define uh_dport dest
#define uh_ulen len
#define uh_sum check
#endif /* linux */
static int
initialize_packets(struct targ *targ)
{
	struct pkt *pkt;
	struct ether_header *eh;
	struct ip *ip;
	struct udphdr *udp;
	uint16_t paylen = targ->g->pkt_size - sizeof(*eh) - sizeof(*ip);
	int i, j, l, l0 = strlen(default_payload);
	int src_ip_range = 0, dst_ip_range = 0;
	uint32_t *ip_addr;

	targ->pkts = calloc(targ->ring_pkts, sizeof(struct pkt *));
	if (targ->pkts == NULL)
		return 1;
	for (i = 0; i < targ->ring_pkts; i++) {
		targ->pkts[i] = calloc(targ->pkt_size, 1);
		if (targ->pkts[i] == NULL)
			return 1;
	}

	src_ip_range = address_range((char *) targ->g->src_ip);
	dst_ip_range = address_range((char *) targ->g->dst_ip);
	for (i = 0; i < targ->ring_pkts; i++) {
		pkt = targ->pkts[i];

		for (j = 0; j < paylen;) {
			l = min(l0, paylen - j);
			bcopy(default_payload, pkt->body + j, l);
			j += l;
		}
		pkt->body[j - 1] = '\0';

		ip = &pkt->ip;
		ip->ip_v = IPVERSION;
		ip->ip_hl = 5;
		ip->ip_id = 0;
		ip->ip_tos = IPTOS_LOWDELAY;
		ip->ip_len = ntohs(targ->g->pkt_size - sizeof(*eh));
		ip->ip_id = 0;
		ip->ip_off = htons(IP_DF); /* Don't fragment */
		ip->ip_ttl = IPDEFTTL;
		ip->ip_p = IPPROTO_UDP;
		inet_aton(targ->g->src_ip, (struct in_addr *)&ip->ip_src);
		if (src_ip_range > 1) {
			ip_addr = (uint32_t *) &ip->ip_src.s_addr;
			*ip_addr += (i % src_ip_range) << 24;
		}
		targ->dst_ip = ip->ip_dst.s_addr;
		inet_aton(targ->g->dst_ip, (struct in_addr *)&ip->ip_dst);
		if (dst_ip_range > 1) {
			ip_addr = (uint32_t *) &ip->ip_dst.s_addr;
			*ip_addr += (i % dst_ip_range) << 24;
		}
		targ->src_ip = ip->ip_src.s_addr;
		ip->ip_sum = 0;
		ip->ip_sum = wrapsum(checksum(ip, sizeof(*ip), 0));

		udp = &pkt->udp;
		udp->uh_sport = htons(4096);
		udp->uh_dport = htons(8192);
		udp->uh_ulen = htons(paylen);
		udp->uh_sum = 0;
		/* Magic: taken from sbin/dhclient/packet.c */
		udp->uh_sum = wrapsum(checksum(udp, sizeof(*udp),
		    checksum(pkt->body,
			targ->g->pkt_size - sizeof(*eh) - sizeof(*ip) - sizeof(*udp),
			checksum(&ip->ip_src, 2 * sizeof(ip->ip_src),
			    IPPROTO_UDP + (u_int32_t)ntohs(udp->uh_ulen)
			)
		    )
		));

		eh = &pkt->eh;
		bcopy(ether_aton(targ->g->src_mac), targ->src_mac, 6);
		bcopy(targ->src_mac, eh->ether_shost, 6);
#if 0
		p = index(targ->g->src_mac, '%');
		if (p)
			targ->src_mac_range = atoi(p+1);
#endif

		bcopy(ether_aton(targ->g->dst_mac), targ->dst_mac, 6);
		bcopy(targ->dst_mac, eh->ether_dhost, 6);
#if 0
		p = index(targ->g->dst_mac, '%');
		if (p)
			targ->dst_mac_range = atoi(p+1);
#endif
		eh->ether_type = htons(ETHERTYPE_IP);
	}

	return 0;
}

/* Check the payload of the packet for errors (use it for debug).
 * Look for consecutive ascii representations of the size of the packet.
 */
static void
check_payload(char *p, int psize)
{
	char temp[64];
	int n_read, size, sizelen;

	/* get the length in ASCII of the length of the packet. */
	sizelen = sprintf(temp, "%d", psize) + 1; // include a whitespace

	/* dummy payload. */
	p += 14; /* skip packet header. */
	n_read = 14;
	while (psize - n_read >= sizelen) {
		sscanf(p, "%d", &size);
		if (size != psize) {
			D("Read %d instead of %d", size, psize);
			break;
		}

		p += sizelen;
		n_read += sizelen;
	}
}


/*
 * create and enqueue a batch of packets on a ring.
 * On the last one set NS_REPORT to tell the driver to generate
 * an interrupt when done.
 */
static int
send_packets(struct netmap_ring *ring, struct pkt **pkts,
		int size, u_int count, int copy_all)
{
	u_int sent, cur = ring->cur;

	if (ring->avail < count)
		count = ring->avail;

	for (sent = 0; sent < count; sent++) {
		struct netmap_slot *slot = &ring->slot[cur];
		char *p = NETMAP_BUF(ring, slot->buf_idx);

		if (copy_all)
			memcpy(p, pkts[cur], size);

		slot->len = size;
		if (sent == count - 1)
			slot->flags |= NS_REPORT;
		cur = NETMAP_RING_NEXT(ring, cur);
	}
	ring->avail -= sent;
	ring->cur = cur;

	return (sent);
}

static void *
sender_body(void *data)
{
	struct targ *targ = (struct targ *) data;

	struct pollfd fds[1];
	struct netmap_if *nifp = targ->nifp;
	struct netmap_ring *txring;
	int i, n = targ->g->npackets / targ->g->nthreads, sent = 0;
	int fill_all = 1;

	if (setaffinity(targ->thread, targ->affinity))
		goto quit;
	/* setup poll(2) mechanism. */
	memset(fds, 0, sizeof(fds));
	fds[0].fd = targ->fd;
	fds[0].events = (POLLOUT);

	/* main loop.*/
	gettimeofday(&targ->tic, NULL);
    if (targ->g->use_pcap) {
	int size = targ->g->pkt_size;
	void *pkt = targ->pkts;
	pcap_t *p = targ->g->p;

	for (; sent < n; sent++) {
		if (pcap_inject(p, pkt, size) == -1)
			break;
	}
    } else {
	while (sent < n) {

		/*
		 * wait for available room in the send queue(s)
		 */
		if (poll(fds, 1, 2000) <= 0) {
			D("poll error/timeout on queue %d\n", targ->me);
			goto quit;
		}

		/*
		 * we need to fill *all* the packets of *all* the queues at
		 * least once.Once we are done, set to copy_all global
		 * parameter.
		 */
		if (sent > 100000)
			fill_all = targ->g->copy_all;

		/*
		 * scan our queues and send on those with room
		 */
		for (i = targ->qfirst; i < targ->qlast; i++) {
			int m, limit = MIN(n - sent, targ->g->burst);

			txring = NETMAP_TXRING(nifp, i);
			if (txring->avail == 0)
				continue;
			m = send_packets(txring, targ->pkts, targ->g->pkt_size,
					 limit, fill_all);
			sent += m;
			targ->count = sent;
		}
	}
	/* Tell the interface that we have new packets. */
	ioctl(fds[0].fd, NIOCTXSYNC, NULL);

	/* final part: wait all the TX queues to be empty. */
	for (i = targ->qfirst; i < targ->qlast; i++) {
		txring = NETMAP_TXRING(nifp, i);
		while (!NETMAP_TX_RING_EMPTY(txring)) {
			ioctl(fds[0].fd, NIOCTXSYNC, NULL);
			usleep(1); /* wait 1 tick */
		}
	}
    }

	gettimeofday(&targ->toc, NULL);
	targ->completed = 1;
	targ->count = sent;

quit:
	/* reset the ``used`` flag. */
	targ->used = 0;

	return (NULL);
}


static void
receive_pcap(u_char *user, __unused const struct pcap_pkthdr * h,
	__unused const u_char * bytes)
{
	int *count = (int *)user;
	(*count)++;
}

static int
receive_packets(struct netmap_ring *ring, u_int limit, int copy_all,
		int skip_payload)
{
	u_int cur, rx;
	char pbuf[2048];

	cur = ring->cur;
	if (ring->avail < limit)
		limit = ring->avail;
	for (rx = 0; rx < limit; rx++) {
		struct netmap_slot *slot = &ring->slot[cur];
		char *p = NETMAP_BUF(ring, slot->buf_idx);

		if (copy_all)
			memcpy(pbuf, p, slot->len);
		if (!skip_payload)
			check_payload(p, slot->len);

		cur = NETMAP_RING_NEXT(ring, cur);
	}
	ring->avail -= rx;
	ring->cur = cur;

	return (rx);
}

static void *
receiver_body(void *data)
{
	struct targ *targ = (struct targ *) data;
	struct pollfd fds[1];
	struct netmap_if *nifp = targ->nifp;
	struct netmap_ring *rxring;
	int i, received = 0;

	if (setaffinity(targ->thread, targ->affinity))
		goto quit;

	/* setup poll(2) mechanism. */
	memset(fds, 0, sizeof(fds));
	fds[0].fd = targ->fd;
	fds[0].events = (POLLIN);

	/* unbounded wait for the first packet. */
	for (;;) {
		i = poll(fds, 1, 1000);
		if (i > 0 && !(fds[0].revents & POLLERR))
			break;
		D("waiting for initial packets, poll returns %d %d", i, fds[0].revents);
	}

	/* main loop, exit after 1s silence */
	gettimeofday(&targ->tic, NULL);
    if (targ->g->use_pcap) {
	for (;;) {
		pcap_dispatch(targ->g->p, targ->g->burst, receive_pcap, NULL);
	}
    } else {
	while (1) {
		/* Once we started to receive packets, wait at most 1 seconds
		   before quitting. */
		if (poll(fds, 1, 1 * 1000) <= 0) {
			gettimeofday(&targ->toc, NULL);
			targ->toc.tv_sec -= 1; /* Subtract timeout time. */
			break;
		}

		for (i = targ->qfirst; i < targ->qlast; i++) {
			int m;

			rxring = NETMAP_RXRING(nifp, i);
			if (rxring->avail == 0)
				continue;

			m = receive_packets(rxring, targ->g->burst,
					targ->g->copy_all, SKIP_PAYLOAD);
			received += m;
			targ->count = received;
		}

		// tell the card we have read the data
		//ioctl(fds[0].fd, NIOCRXSYNC, NULL);
	}
    }

	targ->completed = 1;
	targ->count = received;

quit:
	/* reset the ``used`` flag. */
	targ->used = 0;

	return (NULL);
}

static void
tx_output(uint64_t sent, int size, double delta)
{
	double amount = 8.0 * (1.0 * size * sent) / delta;
	double pps = sent / delta;
	char units[4] = { '\0', 'K', 'M', 'G' };
	int aunit = 0, punit = 0;

	while (amount >= 1000) {
		amount /= 1000;
		aunit += 1;
	}
	while (pps >= 1000) {
		pps /= 1000;
		punit += 1;
	}

	printf("Sent %" PRIu64 " packets, %d bytes each, in %.2f seconds.\n",
	       sent, size, delta);
	printf("Speed: %.2f%cpps. Bandwidth: %.2f%cbps.\n",
	       pps, units[punit], amount, units[aunit]);
}


static void
rx_output(uint64_t received, double delta)
{

	double pps = received / delta;
	char units[4] = { '\0', 'K', 'M', 'G' };
	int punit = 0;

	while (pps >= 1000) {
		pps /= 1000;
		punit += 1;
	}

	printf("Received %" PRIu64 " packets, in %.2f seconds.\n", received, delta);
	printf("Speed: %.2f%cpps.\n", pps, units[punit]);
}

static void
usage(void)
{
	const char *cmd = "pkt-gen";
	fprintf(stderr,
		"Usage:\n"
		"%s arguments\n"
		"\t-i interface		interface name\n"
		"\t-t pkts_to_send	also forces send mode\n"
		"\t-r pkts_to_receive	also forces receive mode\n"
		"\t-l pkts_size		in bytes excluding CRC\n"
		"\t-d dst-ip		end with %%n to sweep n addresses\n"
		"\t-s src-ip		end with %%n to sweep n addresses\n"
		"\t-D dst-mac		end with %%n to sweep n addresses\n"
		"\t-S src-mac		end with %%n to sweep n addresses\n"
		"\t-a cpu_id		use setaffinity\n"
		"\t-b burst size		testing, mostly\n"
		"\t-c cores		cores to use\n"
		"\t-p threads		processes/threads to use\n"
		"\t-T report_ms		milliseconds between reports\n"
		"\t-w wait_for_link_time	in seconds\n"
		"\t-C			copy all packets\n"
		"",
		cmd);

	exit(0);
}


int
main(int arc, char **argv)
{
	int i, fd;

	struct glob_arg g;

	struct nmreq nmr;
	void *mmap_addr;		/* the mmap address */
	void *(*td_body)(void *) = receiver_body;
	int ch;
	int report_interval = 1000;	/* report interval */
	char *ifname = NULL;
	int wait_link = 2;
	int devqueues = 1;	/* how many device queues */
	int affinity = -1;

	bzero(&g, sizeof(g));

	g.src_ip = "10.0.0.1";
	g.dst_ip = "10.1.0.1";
	g.dst_mac = "ff:ff:ff:ff:ff:ff";
	g.src_mac = NULL;
	g.pkt_size = 60;
	g.burst = 512;		// default
	g.nthreads = 1;

	while ( (ch = getopt(arc, argv,
			"a:i:t:r:l:d:s:D:S:b:c:p:T:w:Cv")) != -1) {
		switch(ch) {
		default:
			D("bad option %c %s", ch, optarg);
			usage();
			break;
		case 'a':	/* force affinity */
			affinity = atoi(optarg);
			break;
		case 'i':	/* interface */
			ifname = optarg;
			break;
		case 't':	/* send */
			td_body = sender_body;
			g.npackets = atoi(optarg);
			break;
		case 'r':	/* receive */
			td_body = receiver_body;
			g.npackets = atoi(optarg);
			break;
		case 'l':	/* pkt_size */
			g.pkt_size = atoi(optarg);
			break;
		case 'd':
			g.dst_ip = optarg;
			break;
		case 's':
			g.src_ip = optarg;
			break;
		case 'T':	/* report interval */
			report_interval = atoi(optarg);
			break;
		case 'w':
			wait_link = atoi(optarg);
			break;
		case 'b':	/* burst */
			g.burst = atoi(optarg);
			break;
		case 'c':
			g.cpus = atoi(optarg);
			break;
		case 'p':
			g.nthreads = atoi(optarg);
			break;

		case 'P':
			g.use_pcap = 1;
			break;

		case 'D': /* destination mac */
			g.dst_mac = optarg;
	{
		struct ether_addr *mac = ether_aton(g.dst_mac);
		D("ether_aton(%s) gives %p", g.dst_mac, mac);
	}
			break;
		case 'S': /* source mac */
			g.src_mac = optarg;
			break;
		case 'C': /* copy all packets */
			g.copy_all = 1;
			break;
		case 'v':
			verbose++;
		}
	}

	if (ifname == NULL) {
		D("missing ifname");
		usage();
	}
	{
#ifdef __FreeBSD__
		int n = system_ncpus();
#else /* FreeBSD */
		int n = 1;
#endif /* !FreeBSD */
		if (g.cpus < 0 || g.cpus > n) {
			D("%d cpus is too high, have only %d cpus", g.cpus, n);
			usage();
		}
		if (g.cpus == 0)
			g.cpus = n;
	}
	if (g.pkt_size < 16 || g.pkt_size > 1536) {
		D("bad pktsize %d\n", g.pkt_size);
		usage();
	}

	bzero(&nmr, sizeof(nmr));
	nmr.nr_version = NETMAP_API;
	/*
	 * Open the netmap device to fetch the number of queues of our
	 * interface.
	 *
	 * The first NIOCREGIF also detaches the card from the
	 * protocol stack and may cause a reset of the card,
	 * which in turn may take some time for the PHY to
	 * reconfigure.
	 */
	fd = open("/dev/netmap", O_RDWR);
	if (fd == -1) {
		D("Unable to open /dev/netmap");
		// fail later
	} else {
		if ((ioctl(fd, NIOCGINFO, &nmr)) == -1) {
			D("Unable to get if info without name");
		} else {
			D("map size is %d Kb", nmr.nr_memsize >> 10);
		}
		bzero(&nmr, sizeof(nmr));
		nmr.nr_version = NETMAP_API;
		strncpy(nmr.nr_name, ifname, sizeof(nmr.nr_name));
		if ((ioctl(fd, NIOCGINFO, &nmr)) == -1) {
			D("Unable to get if info for %s", ifname);
		}
		devqueues = nmr.nr_rx_rings;
	}

	/* validate provided nthreads. */
	if (g.nthreads < 1 || g.nthreads > devqueues) {
		D("bad nthreads %d, have %d queues", g.nthreads, devqueues);
		// continue, fail later
	}

	if (td_body == sender_body && g.src_mac == NULL) {
		static char mybuf[20] = "ff:ff:ff:ff:ff:ff";
		/* retrieve source mac address. */
		if (source_hwaddr(ifname, mybuf) == -1) {
			D("Unable to retrieve source mac");
			// continue, fail later
		}
		g.src_mac = mybuf;
	}

	/*
	 * Map the netmap shared memory: instead of issuing mmap()
	 * inside the body of the threads, we prefer to keep this
	 * operation here to simplify the thread logic.
	 */
	D("mmapping %d Kbytes", nmr.nr_memsize>>10);
	mmap_addr = (struct netmap_d *) mmap(0, nmr.nr_memsize,
					    PROT_WRITE | PROT_READ,
					    MAP_SHARED, fd, 0);
	if (mmap_addr == MAP_FAILED) {
		D("Unable to mmap %d KB", nmr.nr_memsize >> 10);
		// continue, fail later
	}

	/*
	 * Register the interface on the netmap device: from now on,
	 * we can operate on the network interface without any
	 * interference from the legacy network stack.
	 *
	 * We decide to put the first interface registration here to
	 * give time to cards that take a long time to reset the PHY.
	 */
	nmr.nr_version = NETMAP_API;
	if (ioctl(fd, NIOCREGIF, &nmr) == -1) {
		D("Unable to register interface %s", ifname);
		//continue, fail later
	}


	/* Print some debug information. */
	fprintf(stdout,
		"%s %s: %d queues, %d threads, %d cpus and affinity %d.\n",
		(td_body == sender_body) ? "Sending on" : "Receiving from",
		ifname,
		devqueues,
		g.nthreads,
		g.cpus,
		affinity);
	if (td_body == sender_body) {
		fprintf(stdout, "%s -> %s (%s -> %s)\n",
			g.src_ip, g.dst_ip,
			g.src_mac, g.dst_mac);
	}

	/* Exit if something went wrong. */
	if (fd < 0) {
		D("aborting");
		usage();
	}


	/* Wait for PHY reset. */
	D("Wait %d secs for phy reset", wait_link);
	sleep(wait_link);
	D("Ready...");

	/* Install ^C handler. */
	global_nthreads = g.nthreads;
	signal(SIGINT, sigint_h);

	if (g.use_pcap) {
		// XXX g.p = pcap_open_live(..);
	}

	targs = calloc(g.nthreads, sizeof(*targs));
	/*
	 * Now create the desired number of threads, each one
	 * using a single descriptor.
 	 */
	for (i = 0; i < g.nthreads; i++) {
		struct netmap_if *tnifp;
		struct netmap_ring *tring;
		struct nmreq tifreq;
		int tfd;

	    if (g.use_pcap) {
		tfd = -1;
		tnifp = NULL;
		tring = NULL;
	    } else {
		/* register interface. */
		tfd = open("/dev/netmap", O_RDWR);
		if (tfd == -1) {
			D("Unable to open /dev/netmap");
			continue;
		}

		bzero(&tifreq, sizeof(tifreq));
		strncpy(tifreq.nr_name, ifname, sizeof(tifreq.nr_name));
		tifreq.nr_ringid = (g.nthreads > 1) ? (i | NETMAP_HW_RING) : 0;

		/*
		 * if we are acting as a receiver only, do not touch the transmit ring.
		 * This is not the default because many apps may use the interface
		 * in both directions, but a pure receiver does not.
		 */
		if (td_body == receiver_body) {
			tifreq.nr_ringid |= NETMAP_NO_TX_POLL;
		}

		tifreq.nr_version = NETMAP_API;
		if ((ioctl(tfd, NIOCREGIF, &tifreq)) == -1) {
			D("Unable to register %s", ifname);
			continue;
		}
		tnifp = NETMAP_IF(mmap_addr, tifreq.nr_offset);
		tring = NETMAP_TXRING(tnifp, 0);
	    }
		/* start threads. */
		bzero(&targs[i], sizeof(targs[i]));
		targs[i].g = &g;
		targs[i].used = 1;
		targs[i].completed = 0;
		targs[i].fd = tfd;
		targs[i].nmr = tifreq;
		targs[i].nifp = tnifp;
		targs[i].qfirst = (g.nthreads > 1) ? i : 0;
		targs[i].qlast = (g.nthreads > 1) ? i+1 :
			(td_body == receiver_body ? tifreq.nr_rx_rings : tifreq.nr_tx_rings);
		targs[i].me = i;
		targs[i].ring_pkts = tring->num_slots;
		targs[i].pkt_size = tring->nr_buf_size;
		if (affinity >= 0) {
			if (affinity < g.cpus)
				targs[i].affinity = affinity;
			else /* affinity >= g.cpus */
				targs[i].affinity = i % g.cpus;
		} else
			targs[i].affinity = -1;
		if (td_body == sender_body) {
			/* initialize the packet to send. */
			if (initialize_packets(&targs[i])) {
				D("Unable to initialize packets for thread %d",
				    i);
				targs[i].used = 0;
				continue;
			}
		}

		if (pthread_create(&targs[i].thread, NULL, td_body,
				   &targs[i]) == -1) {
			D("Unable to create thread %d", i);
			targs[i].used = 0;
		}
	}

    {
	uint64_t my_count = 0, prev = 0;
	uint64_t count = 0;
	double delta_t;
	struct timeval tic, toc;

	gettimeofday(&toc, NULL);
	for (;;) {
		struct timeval now, delta;
		uint64_t pps;
		int done = 0;

		delta.tv_sec = report_interval/1000;
		delta.tv_usec = (report_interval%1000)*1000;
		select(0, NULL, NULL, NULL, &delta);
		gettimeofday(&now, NULL);
		timersub(&now, &toc, &toc);
		my_count = 0;
		for (i = 0; i < g.nthreads; i++) {
			my_count += targs[i].count;
			if (targs[i].used == 0)
				done++;
		}
		pps = toc.tv_sec* 1000000 + toc.tv_usec;
		if (pps < 10000)
			continue;
		pps = (my_count - prev)*1000000 / pps;
		D("%" PRIu64 " pps", pps);
		prev = my_count;
		toc = now;
		if (done == g.nthreads)
			break;
	}

	timerclear(&tic);
	timerclear(&toc);
	for (i = 0; i < g.nthreads; i++) {
		/*
		 * Join active threads, unregister interfaces and close
		 * file descriptors.
		 */
		pthread_join(targs[i].thread, NULL);
		ioctl(targs[i].fd, NIOCUNREGIF, &targs[i].nmr);
		close(targs[i].fd);

		if (targs[i].pkts != NULL) {
			int j;
			for (j = 0; j < targs[i].ring_pkts; j++) {
				if (targs[i].pkts[j] != NULL) {
					free(targs[i].pkts[j]);
				}
			}
			free(targs[i].pkts);
		}

		if (targs[i].completed == 0)
			continue;

		/*
		 * Collect threads output and extract information about
		 * how long it took to send all the packets.
		 */
		count += targs[i].count;
		if (!timerisset(&tic) || timercmp(&targs[i].tic, &tic, <))
			tic = targs[i].tic;
		if (!timerisset(&toc) || timercmp(&targs[i].toc, &toc, >))
			toc = targs[i].toc;
	}

	/* print output. */
	timersub(&toc, &tic, &toc);
	delta_t = toc.tv_sec + 1e-6* toc.tv_usec;
	if (td_body == sender_body)
		tx_output(count, g.pkt_size, delta_t);
	else
		rx_output(count, delta_t);
    }

	ioctl(fd, NIOCUNREGIF, &nmr);
	munmap(mmap_addr, nmr.nr_memsize);
	close(fd);

	return (0);
}
/* end of file */
