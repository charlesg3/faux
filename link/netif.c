#define _GNU_SOURCE

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <net/if.h>
#include <poll.h>
#include <assert.h>


#include <net/netif.h>
#include <netmap_new/netmap_user.h>
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <core/core.h>

void netif_info(netif_info_t *out_info, const char *iname)
{
    struct nmreq nmr;
    int r;

    bzero(out_info, sizeof(netif_info_t));

    int fd = open("/dev/netmap", O_RDWR);
    if(fd < 0)
    {
        DEBUG("NIF open failed for stat, %d\n", errno);
        perror("errno");
        return;
    }

    bzero(&nmr, sizeof(nmr));
    strcpy(nmr.nr_name, iname);

    nmr.nr_ringid = 0;
    nmr.nr_version = NETMAP_API;

    r = ioctl(fd, NIOCGINFO, &nmr);
    if(r != 0)
    {
        DEBUG("Get info failed, %d\n", errno);
        goto close_and_ret;
    }

    out_info->valid = 1;
    out_info->tx_rings = nmr.nr_tx_rings;
    out_info->rx_rings = nmr.nr_rx_rings;
    out_info->tx_slots = nmr.nr_tx_slots;
    out_info->rx_slots = nmr.nr_rx_slots;

close_and_ret:
    close(fd);

    return;
}

int netif_init(netif_t *out_if,const char *iname, int ringnum)
{
    struct nmreq nmr;
    int r;

    char *path = "/dev/netmap";
    
    
    // Check that the interface is up and ready
    // We need a temporary sock so we have an
    // fd to ioctl on
    int tsock = socket(AF_INET, SOCK_DGRAM, 0);
    struct ifreq ifr;
    if(tsock == -1) 
    {
        DEBUG("Failed to open socket to test iface");
        return -EBUG;
    }
    
    strncpy(ifr.ifr_name, iname, IFNAMSIZ);
    if(ioctl(tsock, SIOCGIFFLAGS, &ifr) < 0)
    {
        DEBUG("ioctl failed on named interface %s\n", iname);
        close(tsock);
        return -EBADIF;
    }

    if((ifr.ifr_flags & IFF_UP) == 0)
    {
        DEBUG("Interface %s is not up\n", iname);
        close(tsock);
        return -EIFDOWN;
    }
    close(tsock);

    // We know that the interface is up, continue
    // with netmap setup

    int fd = open(path, O_RDWR);
    if(fd < 0)
    {
        DEBUG("NIF open failed, %d (%s)\n", errno, path);
        perror("errno");
        return -errno;
    }


    // Initialize netmap request
    bzero(&nmr, sizeof(nmr));
    strcpy(nmr.nr_name, iname);

    // Actual hardware ring binding happens with an OR of
    // NETMAP_HW_RING and the ring number
    nmr.nr_ringid = NETMAP_HW_RING | ringnum;


    // Mark request as valid
    nmr.nr_version = NETMAP_API;

    // Register interface
    r = ioctl(fd, NIOCREGIF, &nmr);
    if(r != 0)
    {
        close(fd);
        return -r;
    }

    // Map shared kernel/user area
    out_if->shm = mmap(0, nmr.nr_memsize, PROT_WRITE | PROT_READ,
                                          MAP_SHARED, fd, 0);
    if(out_if->shm == MAP_FAILED)
    {
        close(fd);
        return -EMAPFAILED;
    }

    // Setup FDs
    out_if->fd = fd;

    out_if->nif = NETMAP_IF(out_if->shm, nmr.nr_offset);
    strncpy(out_if->name, nmr.nr_name, sizeof(out_if->name));
    out_if->ring = ringnum;


    struct netmap_ring *txr = NETMAP_TXRING(out_if->nif, out_if->ring);
    struct netmap_ring *rxr = NETMAP_RXRING(out_if->nif,out_if->ring);
    DEBUG("Opened %s [ring %d]\nrx/tx slots: %u/%u\nrx/tx rings: %u/%u\n"
          "nmr info:\nrx/tx slots: %u/%u\nrx/tx rings: %u/%u\n\n",
            out_if->name, out_if->ring,
            rxr->num_slots, txr->num_slots,
            out_if->nif->ni_rx_rings,
            out_if->nif->ni_tx_rings,
            nmr.nr_rx_slots,
            nmr.nr_tx_slots,
            nmr.nr_rx_rings,
            nmr.nr_tx_rings);

    return 0;
}


int netif_close(netif_t *in_if)
{
    struct nmreq nmr;
    int r;

    bzero(&nmr, sizeof(nmr));
    nmr.nr_version = NETMAP_API;
    strcpy(nmr.nr_name, in_if->name);

    r = ioctl(in_if->fd, NIOCUNREGIF, &nmr);
    if(r != 0)
        return -r;

    close(in_if->fd);

    return 0;
}

int netif_bind_cpu(netif_t *in_ifs, size_t num_ifs, int cpu)
{
    int r = bind_to_core(cpu);
    if(r != 0)
    {
        return -errno;
    }


    const int buf_size = 1024*128;
    char buf[buf_size];
    memset(buf, 0, buf_size);
    size_t len = 0;
        
    size_t ret;
    FILE *inters = fopen("/proc/interrupts", "r");
    assert(inters);
    while((ret = fread(buf+len, buf_size-len, 1, inters)) > 0 && len < buf_size-1)
    {
        len += ret;
    }


    char *line = buf;
    char *end = strstr(buf, "\n");

    int infolen = 0;

    struct 
    {
        int irq;
        int ring;
    } info[64];


    char search[1024];
    snprintf(search, 1024, "%s-TxRx-", in_ifs[0].name);

    while(end != NULL)
    {
        end[0] = '\0';
        end++;

        char *match;
        if(match = strstr(line, search))
        {
            int ring = atoi(match+strlen(search));
            int irq = atoi(line);

            info[infolen].irq = irq;
            info[infolen].ring = ring;
            infolen++;
        }


        line = end;
        end = strstr(end, "\n");
    }

    int i;
    for(i=0; i<num_ifs; i++)
    {
        int j;
        int ok = 0;
        for(j=0; j<infolen; j++)
        {
            if(info[j].ring == in_ifs[i].ring)
            {
                uint16_t high = 0;
                uint32_t mid = 0;
                uint32_t low = 0;

                if(cpu >= 64)
                    high = 1 << (cpu-64);
                else if(cpu >= 32)
                    mid = 1 << (cpu-32);
                else 
                    low = 1 << (cpu);

                char file[256];
                char value[256];
                snprintf(file, 256, "/proc/irq/%d/smp_affinity", info[j].irq);
                snprintf(value, 256, "%04x,%08x,%08x", high, mid, low);

                DEBUG("%s to %s (pin irq %d for ring %d to cpu %d)\n", value, file,
                        info[j].irq, info[j].ring, cpu);
                FILE *f = fopen(file, "w");
                assert(f);
                fwrite(value, strlen(value), 1, f);
                fclose(f);


                ok = 1;
                break;
            }
        }
        assert(ok);
    }


    return 0;
}


int netif_set_flow_groups(netif_t in_ifs[], size_t num_fds, uint16_t start, 
        uint16_t num, uint16_t skip)
{
    size_t i;
    uint16_t cur = start;
    for(i = 0; i<num; i++)
    {
        struct netmap_if_flow flow;
        bzero(&flow, sizeof(flow));

        netif_t *nif = &in_ifs[i % num_fds];

        // Check that flow group id is valid
        if(cur > NETIF_NUM_FLOW_GROUPS)
            break;

        // Set up flow group info and change flow mapping
        flow.f_id = cur;
        cur += skip;
        flow.f_ring = nif->ring;

        // NOTE: Never allow remapping of flow 67
        // DHCP must always map to ring 0
        if(flow.f_id == 67 && flow.f_ring != 0)
        {
            DEBUG("WARNING: Remapping of flow 67 overridden to protect DHCP\n");
            continue;
        }

        int r = ioctl(nif->fd, NIOCADDFLOW, &flow);
        if(r != 0)
            return r;

        DEBUG("Mapped flow %lu to ring %lu\n", flow.f_id, flow.f_ring);
    }

    return 0;
}

int netif_querytx(netif_t *in_if)
{
    int ret = 0;
    struct netmap_ring *txr = NETMAP_TXRING(in_if->nif, in_if->ring);
    ret = !NETMAP_TX_RING_EMPTY(txr);
    return ret;
}


#define MAX(x,y) (x>y)?(x):(y)

int netif_synctx(netif_t *in_if)
{
    int ret = ioctl(in_if->fd, NIOCTXSYNC, NULL);

    return ret;
}

int netif_send(netif_t *in_if, char *data, size_t len)
{
    netif_quicksend(in_if, data, len);

    int r = ioctl(in_if->fd, NIOCTXSYNC, NULL);
    if(r != 0)
    {
        DEBUG("Failed to sync send ring");
        return -ESYNCFAILED;
    }

    return 0;
}

int netif_quicksend(netif_t *in_if, char *data, size_t len)
{
    int r;
    struct netmap_ring *txring = NETMAP_TXRING(in_if->nif, in_if->ring);

    // If no slots are available, poll until some are available
    // BUG: Will this even work if we set NETMAP_NO_TX_POLL?
    if(txring->avail == 0)
    {

        //DEBUG("Polling for send on %s\n", in_if->name); 
        struct pollfd pfd;
        pfd.fd = in_if->fd;
        pfd.events = POLLOUT;
        r = poll(&pfd, 1, -1);

        if(r != 1)
        {
            DEBUG("Poll gave error %d\n", errno);
            return -errno;
        }

        if(pfd.revents != POLLOUT)
        {
            DEBUG("There was an fd error\n");
            return -EFDERROR;
        }

        //DEBUG("Got %d available\n", txring->avail);
    }

    if(txring->avail == 0)
    {
        DEBUG("Found a bug in netif send\n");
        return -EBUG;
    }

    if(len > txring->nr_buf_size)
    {
        DEBUG("Could not send packet, size %d > max %d\n", (int)len, (int)txring->nr_buf_size);
        return -EINVAL;
    }

    // Put the data in the buffer for the slot
    int cur = txring->cur;
    char *buf = NETMAP_BUF(txring, txring->slot[cur].buf_idx);
    memcpy(buf, data, len);

    // Update slot and ring info
    txring->slot[cur].len = len;
    txring->cur = NETMAP_RING_NEXT(txring, cur);
    txring->avail -= 1;
    //txring->flags = NS_REPORT;

    return 0;
}


int netif_poll(netif_t *in_if, int timeout)
{

    int r;
    struct netmap_ring *rxring = NETMAP_RXRING(in_if->nif, in_if->ring);

    struct pollfd pfd;
    pfd.fd = in_if->fd;
    pfd.events = POLLIN;
    r = poll(&pfd, 1, timeout);

    if(r < 0)
    {
        DEBUG("Poll gave error %d\n", errno);
        return -errno;
    }

    if((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
    {
        DEBUG("Poll got error event %d\n", pfd.revents);
        return -EFDERROR;
    }

    return rxring->avail;

}


int netif_select(netif_t in_ifs[], netif_t *out_ifs[], int out_nums[], 
        int num_ifs, struct timeval *timeout)
{
    if(num_ifs == 0)
        return -EINVAL;

    fd_set fdset;
    FD_ZERO(&fdset);

    int i;
    int maxfd = in_ifs[0].fd;
    // Setup the fdset
    for(i = 0; i<num_ifs; i++)
    {
        FD_SET(in_ifs[i].fd, &fdset);
        maxfd = MAX(maxfd, in_ifs[i].fd);
    }

    maxfd += 1;

    int ret = select(maxfd, &fdset, NULL, NULL, timeout);

    if(ret > 0)
    {
        // Iterate and find all fds that have data and fill in structures
        int found = 0;
        for(i=0; i<num_ifs; i++)
        {
            if(FD_ISSET(in_ifs[i].fd, &fdset))
            {
                struct netmap_ring *rxring = NETMAP_RXRING(in_ifs[i].nif, in_ifs[i].ring);
                out_ifs[found] = &in_ifs[i];
                out_nums[found] = rxring->avail;
                
                if(++found == ret)
                    break;
            }
        }
    }
    else if(ret < 0)
    {
        // Handle the case where the syscall was interrupted and return no packets
        if(errno == EINTR)
            return 0;
        return -errno;
    }


    return ret;
}



#define MIN(x,y) (x<y)?(x):(y)

int netif_recv(netif_t *in_if, netif_packet_t packets[],
        size_t max_packets)
{
    int r;
    struct netmap_ring *rxring = NETMAP_RXRING(in_if->nif, in_if->ring);


    // Poll for packets if none exist
    if(rxring->avail == 0)
    {
        r = netif_poll(in_if, -1);
        if(r < 0)
        {
            // Handle interrupted system call
            if(errno == EINTR)
                return 0;
            return r;
        }
        if(r == 0)
        {
            DEBUG("BUG: No packets found in recv->poll\n");
        }
    }

    // If there are any packets waiting, get as many as 
    // we can up to max_packets
    size_t num_packets = MIN(rxring->avail, max_packets);
    //DEBUG("Got %d packets of %d avail, max %d\n", num_packets, rxring->avail, max_packets);
    if(num_packets != 0)
    {
        int i;
        int cur = rxring->cur;
        for(i = 0; i < num_packets; i++)
        {
            packets[i].len = &rxring->slot[cur].len;
            packets[i].data = NETMAP_BUF(rxring, rxring->slot[cur].buf_idx);
            //DEBUG("Recv packet %d lenp %p, datap %p\n", i, packets[i].len, packets[i].data);
            cur = NETMAP_RING_NEXT(rxring, cur);
        }
        rxring->avail -= num_packets;
        rxring->cur = cur;
    }

    return num_packets;

}


int netif_get_transmit_buffers(netif_t *in_if, netif_packet_t packets[],
        size_t num_packets)
{
    int r;
    struct netmap_ring *txring = NETMAP_TXRING(in_if->nif, in_if->ring);

    //DEBUG("getting buffers\n");

    // Sync if not enough buffers
    if(num_packets > txring->avail)
    {
        r = ioctl(in_if->fd, NIOCTXSYNC, NULL);
        if(r != 0)
        {
            DEBUG("Sync failed, got %d\n", errno);
            return -ESYNCFAILED;
        }
    }

    // Get as many of the requested buffers as possible
    num_packets = MIN(txring->avail, num_packets);
    //DEBUG("getting %d of %d\n", num_packets, txring->avail);

    int i;
    int cur = txring->cur;
    for(i=0; i<num_packets; i++)
    {
        packets[i].len = &txring->slot[cur].len;
        packets[i].data = NETMAP_BUF(txring, txring->slot[cur].buf_idx);
        cur = NETMAP_RING_NEXT(txring, cur);
    }


    return num_packets;
}

int netif_send_transmit_buffers(netif_t *in_if, size_t num_packets)
{
    int r;
    struct netmap_ring *txring = NETMAP_TXRING(in_if->nif, in_if->ring);

    // Increment up the current pointer
    int i;
    for(i=0; i<num_packets; i++)
        txring->cur = NETMAP_RING_NEXT(txring, txring->cur);

    // Subtract the available packets
    txring->avail -= num_packets;

    //DEBUG("tx sync\n");
    r = ioctl(in_if->fd, NIOCTXSYNC, NULL);
    if(r != 0)
    {
        DEBUG("Sync failed, got %d\n", errno);
        return -ESYNCFAILED;
    }

    return 0;
}

int netif_set_mac(netif_t *in_if, unsigned char *macaddress)
{
	int r = ioctl(in_if->fd, NIOCSETMAC, macaddress);
	if(r != 0)
	    return r;

    DEBUG("Set MAC to %lx\n", *(uint64_t *)macaddress );

	return 0;
}

int netif_get_mac(netif_t *in_if, unsigned char *macaddress)
{
    int r = ioctl(in_if->fd, NIOCGETMAC, macaddress);
    if(r != 0)
        return r;

    DEBUG("Got MAC %lx\n", *(uint64_t *)macaddress );

    return 0;
}

int netif_quickpoll(netif_t in_ifs[], size_t count)
{
    int ret = 0;

    int i;
    for(i = 0; i < count; i++)
    {
        struct netmap_ring *ring = NETMAP_RXRING(in_ifs[i].nif, in_ifs[i].ring);
        ret += ring->waiting_packets;
    }

    return ret;
}



