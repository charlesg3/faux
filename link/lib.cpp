#include <unistd.h>

#include <config/config.h>
#include <utilities/tsc.h>

#include <name/name.h>
#include <messaging/dispatch.h>
#include <utilities/debug.h>
#include <sched.h>

#include <net/netif.h>
#include <net/fauxlink.h>
#include "linkserver/linkserver_private.h"

NetworkPacketList::NetworkPacketList()
{
    on = -1;
    len = 0;
    packets = new netif_packet_t[FAUX_LINK_MAX_PACKETS];
    chanpackets = new struct NetlinkPacket[FAUX_LINK_MAX_PACKETS];
    for(int i=0; i<FAUX_LINK_MAX_PACKETS; i++)
    {
        chanpackets[i].data = NULL;
        chanpackets[i].len = 0;
    }
}

NetworkPacketList::~NetworkPacketList()
{
    delete[] packets;
    delete[] chanpackets;
}

int NetworkPacketList::count()
{
    return len;
}

bool NetworkPacketList::next()
{
    on += 1;
    if(on > len)
        on = len;
    if(on == len)
        return false;
    else
        return true;
}

void NetworkPacketList::get(char **buffer, uint16_t *lenout)
{
    if(on < 0 || on >= len)
    {
        *buffer = NULL;
        *lenout = 0;
    }
    else
    {
        *buffer = packets[on].data;
        *lenout = *packets[on].len;
    }
}

NetworkLink::NetworkLink()
{
}

NetworkLink::~NetworkLink()
{
}

void NetworkLink::tick()
{
    // Do nothing by default
}

LocalNetworkLink::LocalNetworkLink(const char *iface, int ring, int numrings)
{
    nifs = new netif_t[numrings];
    num_nifs = numrings;

    for(int i = 0; i<numrings; i++)
    {
        int r = netif_init(&nifs[i], iface, ring+i);
        if(r != 0)
            throw new NetworkLinkException("Failed to init interface.", r);
    }

    // Re-enable this to test affinity
    //netif_bind_cpu(nifs, num_nifs, sched_getcpu());
    sleep(3);
}

LocalNetworkLink::~LocalNetworkLink()
{
    for(int i=0; i<num_nifs; i++)
    {
        netif_close(&nifs[i]);
    }

    delete[] nifs;
}

void LocalNetworkLink::getmac(unsigned char *buffer)
{
    int r = netif_get_mac(&nifs[0], buffer);
    if(r != 0)
        throw new NetworkLinkException("Failed to get mac", r);
}

void LocalNetworkLink::tick()
{
    for(int i=0; i<num_nifs; i++)
    {
        if(netif_querytx(&nifs[i]))
        {
            int r = netif_synctx(&nifs[i]);
            if(r != 0)
                throw new NetworkLinkException("Explicit TX sync failed", r);
        }
    }
}

int LocalNetworkLink::send(char *packet, size_t len)
{
    int r = netif_quicksend(&nifs[0], packet, len);
    return r;
}

#define MIN(x,y) ((x<y)?(x):(y))

int LocalNetworkLink::receive(NetworkPacketList *packetList, 
        struct timeval *timeout, int max_packets)
{
    netif_t *outifs[num_nifs];
    int out_nums[num_nifs];
    int waitingifs = netif_select(nifs, outifs, out_nums, num_nifs, timeout);
    if(waitingifs < 0)
        return waitingifs;


    netif_packet_t *packets = getPacketPointer(packetList);

    int ret = 0;
    for(int i = 0; i < waitingifs; i++)
    {
        // Short circuit if we have too many packets waiting
        if(ret + out_nums[i] > FAUX_LINK_MAX_PACKETS)
            break;

        int r = netif_recv(outifs[i], packets+ret, MIN(out_nums[i], max_packets-ret));
        //printf("Found %d packets on ring %d (there were %d waiting)\n", r, outifs[i]->ring, out_nums[i]);
        //fflush(stdout);
        if(r < 0)
            return r;
        else
            ret += r;

        if(ret == max_packets)
            break;
    }

    setNumPackets(packetList, ret);
    packetList->reset();

    return ret;
}

int LocalNetworkLink::poll()
{
    int ret = netif_quickpoll(nifs, num_nifs);
    return ret;
}


unsigned int LocalNetworkLink::getTotalFlowGroupCount()
{
    return NETIF_NUM_FLOW_GROUPS;
}

void LocalNetworkLink::setFlowGroups(unsigned int start, unsigned int num, unsigned int skip)
{
    netif_set_flow_groups(nifs, num_nifs, start, num, skip);
}

ServerNetworkLink::ServerNetworkLink(int id, int transport_count)
    : m_id(id)
    , m_transport_count(transport_count)
{
    static Address link_name = 0;
    assert(link_name == 0);

    name_lookup(LINKSERVER_NAME, &link_name);

    m_link_chan = dispatchOpen(link_name);

    DispatchTransaction trans;

    m_ep = endpoint_create();

    LinkRegisterTransportRequest *req;
    req = (LinkRegisterTransportRequest*) dispatchAllocate(m_link_chan, sizeof(LinkRegisterTransportRequest));
    req->id = m_id;
    req->ep_addr = endpoint_get_address(m_ep);
    trans = dispatchRequest(m_link_chan, LINK_REGISTER_TRANSPORT, req, sizeof(*req));

    LinkRegisterTransportResponse *resp;
    __attribute__((unused))
    size_t size = dispatchReceive(trans, (void**)&resp);
    assert(size == sizeof(LinkRegisterTransportResponse));
    assert(resp->ack == 1);
    dispatchFree(trans, resp);

    //Channel Server for packet channel
    while ((m_recv_chan = endpoint_accept(m_ep)) == NULL)
        ;

    printf("Link: setrec endpoint accepted chan = %p\n", (void *)m_recv_chan);
}

ServerNetworkLink::~ServerNetworkLink()
{
    DispatchTransaction trans;

    LinkUnregisterTransportRequest *req;

    req = (LinkUnregisterTransportRequest*) dispatchAllocate(m_link_chan,
            sizeof(LinkUnregisterTransportRequest));
    req->id = m_id;
    trans = dispatchRequest(m_link_chan, LINK_UNREGISTER_TRANSPORT, req, sizeof(*req));

    LinkUnregisterTransportResponse *resp;
    __attribute__((unused))
    size_t size = dispatchReceive(trans, (void**)&resp);
    assert(size == sizeof(LinkUnregisterTransportResponse));
    assert(resp->ack == 1);

    dispatchFree(trans, resp);

    channel_close(m_recv_chan);
    endpoint_destroy(m_ep);
}

void ServerNetworkLink::getmac(unsigned char *buffer)
{
    DispatchTransaction trans;

    void *req = dispatchAllocate(m_link_chan, 1);
    trans = dispatchRequest(m_link_chan, LINK_GET_MAC_ADDRESS, req, 1);

    LinkGetMACResponse *resp;

    __attribute__(unused) size_t size;
    size = dispatchReceive(trans, (void**)&resp);
    assert(size == sizeof(LinkGetMACResponse));
    memcpy(buffer, resp->MAC, 6);
    dispatchFree(trans, resp);

    if(memcmp(buffer, "\x00\x00\x00\x00\x00\x00", 6) == 0)
        throw new NetworkLinkException("Failed to get mac address from linkserver", -1);
}

int ServerNetworkLink::send(char *data, size_t len)
{
    char *req;

    // Retry until alloc succeeds
    while(!(req = (char*) dispatchAllocate(m_link_chan, len)));
    memcpy(req, data, len);
    dispatchRequest(m_link_chan, LINK_SEND_PACKET, req, len);

    return 0;
}

int ServerNetworkLink::receive(NetworkPacketList *packetlist, 
        struct timeval *timeout, int max_packets)
{
    netif_packet_t *packets = getPacketPointer(packetlist);
    NetlinkPacket *chanpackets = getChannelPackets(packetlist);

    int received = 0;

    uint64_t start = ts_to_us(rdtscll());

    bool wait_infinite = true;
    uint64_t wait_us = 0;

    if(max_packets == 0)
        return 0;

    if(timeout)
    {
        wait_infinite = false;
        wait_us = timeout->tv_sec * 1000000 + timeout->tv_usec;
    }


    // Free any previously used buffers in the channel
    for(int i = 0; chanpackets[i].data != NULL && i < FAUX_LINK_MAX_PACKETS; i++)
    {
        //fprintf(stderr, "Freeing %p from channel [index %d]\n", chanpackets[i].data, i);
        channel_free(m_recv_chan, chanpackets[i].data);
        chanpackets[i].data = NULL;
    }

    do
    {
        size_t tempsize;
        void *msg;

        tempsize = channel_poll(m_recv_chan);
        while (tempsize && received < max_packets) 
        {
            tempsize = channel_receive(m_recv_chan, (void **) &msg);

            // Save info on channel message to free later.
            // Also save len here so packet can refer to it
            chanpackets[received].data = msg;
            chanpackets[received].len = tempsize;

            // Set up data in returned packet buffer
            packets[received].data = (char*)msg;
            packets[received].len = &chanpackets[received].len;

            received += 1;

            tempsize = channel_poll(m_recv_chan);
        }

    } while(received == 0 && (wait_infinite || (ts_to_us(rdtscll()) < start+wait_us)));

    if(received != 0)
    {
        setNumPackets(packetlist, received);
        packetlist->reset();
    }

    return received;
}

int ServerNetworkLink::poll()
{
    int size = channel_poll(m_recv_chan);
    if(size)
        return 1;
    else
        return 0;
}


LoopbackNetworkLink::LoopbackNetworkLink(int id, int transport_count)
    : m_id(id)
    , m_transport_count(transport_count)
    , m_received_packets(0)
{

}
LoopbackNetworkLink::~LoopbackNetworkLink()
{

}

void LoopbackNetworkLink::getmac(unsigned char *buffer)
{

}

int LoopbackNetworkLink::send(char *packet, size_t len)
{
    char *data_p = (char *)malloc(len);

    NetlinkPacket *new_packet = (NetlinkPacket *)malloc(sizeof(NetlinkPacket));
    new_packet->data = data_p;
    memcpy(data_p, packet, len);
    new_packet->len = len;
    m_packetBuffer.push_back(new_packet);
    return 0;

}

int LoopbackNetworkLink::receive(NetworkPacketList *packetlist, 
        struct timeval *timeout, int max_packets)
{
    netif_packet_t *packets = getPacketPointer(packetlist);

    int received = 0;

    uint64_t start = ts_to_us(rdtscll());

    bool wait_infinite = true;
    uint64_t wait_us = 0;

    if(max_packets == 0)
        return 0;

    if(timeout)
    {
        wait_infinite = false;
        wait_us = timeout->tv_sec * 1000000 + timeout->tv_usec;
    }


    // Free any previously used buffers in the channel
    for(unsigned int i = 0; i < m_received_packets; i++)
    {
        NetlinkPacket *packet = m_packetBuffer.front();
        m_packetBuffer.pop_front();

        free(packet->data);
        free(packet);
    }

    do
    {
        for (auto i = m_packetBuffer.begin(); i != m_packetBuffer.end(); i++)
        {
            // Set up data in returned packet buffer
            packets[received].data = (char*)((*i)->data);
            packets[received].len = &((*i)->len);

            if(++received >= max_packets)
                break;
        }
    } while(received == 0 && (wait_infinite || (ts_to_us(rdtscll()) < start+wait_us)));

    if(received != 0)
    {
        setNumPackets(packetlist, received);
        packetlist->reset();
    }

    m_received_packets = received;

    return received;

}

int LoopbackNetworkLink::poll()
{
    return m_packetBuffer.size();
}
