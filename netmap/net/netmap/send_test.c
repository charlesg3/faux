#include <errno.h>
#include <signal.h> /* signal */
#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strcmp */
#include <fcntl.h> /* open */
#include <unistd.h> /* close */

#include <sys/ioctl.h> /* ioctl */
#include <arpa/inet.h> /* uint32_t */
#include <sys/mman.h> /* mmap */

#include <sys/poll.h>
#include <net/if.h>	/* ifreq */
#include <net/ethernet.h>
#include <net/netmap.h>
#include <net/netmap_user.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

const char *default_payload = "netmap pkt-gen Luigi Rizzo and Matteo Landi\n"
	"http://info.iet.unipi.it/~luigi/netmap/ ";
const char *default_src_mac = "00:1b:21:44:25:99";
const char *default_dst_mac = "00:1b:21:44:25:98";
const char *default_src_ip = "192.168.1.1";
const char *default_dst_ip = "192.168.1.2";

#define __unused
#define MIN(a, b) ((a) < (b) ? (a) : (b))

int verbose = 1;
#define D(format, ...) do {					\
	if (!verbose) break;					\
	struct timeval _xxts;					\
	gettimeofday(&_xxts, NULL);				\
        fprintf(stderr, "%03d.%06d %s [%d] " format "\n",	\
	(int)_xxts.tv_sec %1000, (int)_xxts.tv_usec,		\
        __FUNCTION__, __LINE__, ##__VA_ARGS__);			\
	} while (0)

struct packet {
	struct ether_header eh;
	struct ip ip;
	struct udphdr udp;
	uint8_t body[2048];
} __attribute__((__packed__));

struct conf {
	int fd;
	int memsize;
	char *dev_name;
	char *mem;
	struct netmap_if *nifp;
	int num_rings;
	int burst;
	int pkt_size;
	struct packet pkt;
};

/* Compute the checksum of the given ip header. */
static uint16_t
checksum(const void *data, uint16_t len)
{
	const uint8_t *addr = data;
	uint32_t sum = 0;

	while (len > 1) {
		sum += addr[0] * 256 + addr[1];
		addr += 2;
		len -= 2;
	}

	if (len == 1)
		sum += *addr * 256;

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	sum = htons(sum);

	return ~sum;
}

/*
 * Fill a packet with some payload.
 */
static void
initialize_packet(struct conf *me)
{
	struct ether_header *eh;
	struct ip *ip;
	struct udphdr *udp;
	uint16_t paylen = me->pkt_size - sizeof(*eh) - sizeof(*ip);
	int i, l, l0 = strlen(default_payload);
	char *p;
	struct packet *pkt = &me->pkt;

	for (i = 0; i < paylen;) {
		l = MIN(l0, paylen - i);
		bcopy(default_payload, pkt->body + i, l);
		i += l;
	}
	pkt->body[i-1] = '\0';

	udp = &pkt->udp;
	udp->source = htons(1234);
	udp->dest = htons(4321);
	udp->len = htons(paylen);
	udp->check = 0; // checksum(udp, sizeof(*udp));

	ip = &pkt->ip;
	ip->ip_v = IPVERSION;
	ip->ip_hl = 5;
	ip->ip_id = 0;
	ip->ip_tos = IPTOS_LOWDELAY;
	ip->ip_len = ntohs(me->pkt_size - sizeof(*eh));
	ip->ip_id = 0;
	ip->ip_off = htons(IP_DF); /* Don't fragment */
	ip->ip_ttl = IPDEFTTL;
	ip->ip_p = IPPROTO_UDP;
	inet_aton(default_src_ip, (struct in_addr *)&ip->ip_src);
	inet_aton(default_dst_ip, (struct in_addr *)&ip->ip_dst);
	ip->ip_sum = checksum(ip, sizeof(*ip));

	eh = &pkt->eh;
	ether_aton_r(default_src_mac, (struct ether_addr*)&eh->ether_shost);
	ether_aton_r(default_dst_mac, (struct ether_addr*)&eh->ether_dhost);
	eh->ether_type = htons(ETHERTYPE_IP);
}

/*
 * create and enqueue a batch of packets on a ring.
 * On the last one set NS_REPORT to tell the driver to generate
 * an interrupt when done.
 */
static int
send_packets(struct netmap_ring *ring, struct packet *pkt, 
		int size, u_int count, int fill_all)
{
	u_int sent, cur = ring->cur;

	if (ring->avail < count)
		count = ring->avail;

	for (sent = 0; sent < count; sent++) {
		struct netmap_slot *slot = &ring->slot[cur];
		char *p = NETMAP_BUF(ring, slot->buf_idx);
		if (fill_all) {
			memcpy(p, pkt, size);
		}

		slot->len = size;
		if (sent == count - 1)
			slot->flags |= NS_REPORT;
		cur = NETMAP_RING_NEXT(ring, cur);
	}
	ring->avail -= sent;
	ring->cur = cur;

	return (sent);
}

int
main(int argc, char *argv[])
{
	struct conf me;
	int err, l, i;
	struct nmreq nmr;
	int sent = 0;
	int fill_all = 1; 
	struct netmap_ring *txring;
	struct pollfd fds[1];
	int n;
	int pause;

	if (argc != 5) {
		D("Bad usage");
		return (-1);
	}
	me.dev_name = argv[1];
	n = atoi(argv[2]);
	me.burst = 1024; // XXX configurable
	me.pkt_size = atoi(argv[3]);
	pause = atoi(argv[4]);

	initialize_packet(&me);

	/* ------------- OPEN --------------*/
	me.fd = open("/dev/netmap", O_RDWR);
	if (me.fd < 0 ) {
		D("Unable to open /dev/netmap");
		return (-1);
	}
	D("open /dev/netmap\t\tok");

	/* ----------- NIOCGINFO -----------*/
	bzero(&nmr, sizeof(nmr));
	nmr.nr_version = NETMAP_API;
	strncpy(nmr.nr_name, me.dev_name, sizeof(nmr.nr_name));
	nmr.nr_ringid = 0;
	err = ioctl(me.fd, NIOCGINFO, &nmr);
	if (err) {
		D("Cannot get info on %s, error: %d (%m)", me.dev_name, err);
		return (-1);
	}
	D("NIOCGINFO\t\tok");
	D("memsize is: %d", nmr.nr_memsize >> 20);
	me.memsize = l = nmr.nr_memsize;

	/* ----------- MMAP -----------*/
	me.mem = mmap(NULL, l, PROT_READ | PROT_WRITE, MAP_SHARED, me.fd, 0);
	if (me.mem == NULL) {
		D("Unable to mmap");
		goto mmap_e;
	}
	D("mmap\t\tok");

	/* ----------- NIOCREGIF -----------*/
	err = ioctl(me.fd, NIOCREGIF, &nmr);
	if (err) {
		D("Unable to register %s", me.dev_name);
		goto ioctl_e;
	}
	D("NIOCREGIF\t\tok");

	me.nifp = NETMAP_IF(me.mem, nmr.nr_offset);
	me.num_rings = nmr.nr_numrings;
	D("numrings: %d", me.num_rings);

	memset(fds, 0, sizeof(fds));
	fds[0].fd = me.fd;
	fds[0].events = (POLLOUT);

	/* ---------- LOOP ------------ */
	D("waiting setup");
	sleep(pause);
	while (sent < n) {
		int ret;

		/*D("calling poll");*/
		ret = poll(fds, 1, 2000);
		if (ret == 0) {
			D("poll timeout");
		} else if(ret < 0) {
			D("poll error");
			break;
		}
		
		for (i = 0; i < me.num_rings; i++) {
			int m, limit = MIN(n - sent, me.burst);

			txring = NETMAP_TXRING(me.nifp, i);
			/*D("ring[%d].avail: %d", i, txring->avail);*/
			if (txring->avail == 0)
				continue;
			m = send_packets(txring, &me.pkt, me.pkt_size,
					 limit, fill_all);
			/*D("sent packets: %d", m);*/
			sent += m;
		}
	}
	D("outside the main loop");

	err = ioctl(me.fd, NIOCTXSYNC, NULL);
	if (err)
		D("error NIOCTXSYNC %d", err);

	D("total sent packets : %d", sent);

	/* ---------- CLEANUP ------------ */
	D("cleaning");
	ioctl(me.fd, NIOCUNREGIF, NULL);
ioctl_e:
	munmap(me.mem, me.memsize);
mmap_e:
	close(me.fd);
	return 0;
}
