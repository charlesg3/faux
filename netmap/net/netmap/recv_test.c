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

static void print_pkt(const void * data) 
{
    struct ether_header *eh;
    struct ip *iph;

    eh = (struct ether_header*)data;
    iph = (struct ip*)((char*)data + sizeof(struct ether_header));
    printf("ETHER SRC: %02x:%02x:%02x:%02x:%02x:%02x ",
            eh->ether_shost[0], eh->ether_shost[1], eh->ether_shost[2],
            eh->ether_shost[3], eh->ether_shost[4], eh->ether_shost[5]);
    printf("DST: %02x:%02x:%02x:%02x:%02x:%02x\n",
            eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
            eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5]);
    printf("TYPE: 0x%04x\n", ntohs(eh->ether_type));
    if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
        printf("IP SRC: %s ", inet_ntoa(iph->ip_src));
        printf("DST: %s\n", inet_ntoa(iph->ip_dst));
    }
    printf("\n");
}

static int
receive_packets(struct netmap_ring *ring, u_int limit, int print)
{
	u_int cur, rx;

	cur = ring->cur;
	if (ring->avail < limit)
		limit = ring->avail;
	for (rx = 0; rx < limit; rx++) {
		struct netmap_slot *slot = &ring->slot[cur];
		if (print) {
			char *p = NETMAP_BUF(ring, slot->buf_idx);
			print_pkt((void *)p);
		}
		cur = NETMAP_RING_NEXT(ring, cur);
	}
	ring->avail -= rx;
	ring->cur = cur;

	return (rx);
}

int
main(int argc, char *argv[])
{
	struct conf me;
	int err, l, i;
	struct nmreq nmr;
	struct netmap_ring *rxring;
	struct pollfd fds[1];
	int recv = 0;
	int n;
	int print = 1;

	if (!strcmp(argv[1], "-h")) {
		D("Help: recv_test iface num_pkts burst [-q]");
		return 1;
	}

	me.dev_name = argv[1];
	n = atoi(argv[2]);
	me.burst = atoi(argv[3]);
	if (argc >= 5 && !strcmp(argv[4], "-q")) {
		D("quite mode");
		print = 0;
	}

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
	nmr.nr_version = NETMAP_API;
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
	fds[0].events = (POLLIN);

	/* ---------- LOOP ------------ */
	D("entering the main loop");
	while (recv < n) {
		int ret;

		D("calling poll");
		ret = poll(fds, 1, 10000);
		if (ret == 0) {
			D("poll timeout");
		} else if(ret < 0) {
			D("poll error");
			break;
		}
		
		for (i = 0; i < me.num_rings; i++) {
			int m;

			rxring = NETMAP_RXRING(me.nifp, i);
			if (rxring->avail == 0)
				continue;

			m = receive_packets(rxring, me.burst, print);
			D("received %d packets", m);
			recv += m;
		}
	}
	D("outside the main loop");

	err = ioctl(me.fd, NIOCRXSYNC, NULL);
	if (err)
		D("error NIOCRXSYNC %d", err);

	D("total recv packets : %d", recv);

	/* ---------- CLEANUP ------------ */
	D("cleaning");
	ioctl(me.fd, NIOCUNREGIF, NULL);
ioctl_e:
	munmap(me.mem, me.memsize);
mmap_e:
	close(me.fd);
	return 0;
}
