#pragma once
#include <string>
#include <string.h>
#include <stdio.h>

#include <messaging/dispatch.h>
#include <net/netif.h>

#define LINKSERVER_NAME 0x2002
enum
{
    LINK_SEND_PACKET,
    LINK_GET_MAC_ADDRESS,
    LINK_REGISTER_TRANSPORT,
    LINK_UNREGISTER_TRANSPORT,
    LINK_QUIT
};

#define FAUX_LINK_MAX_PACKET_LEN 2048
#define FAUX_LINK_MAX_PACKETS 1024

class NetworkLink;


/* 
 * This struct is used to keep track of a single
 * packet that is being passed in a faux channel.
 * Tracking these in NetworkPacketList allows for 
 * delayed free in ServerNetworkLink::receive
 */
struct NetlinkPacket
{
    void *data;
    uint16_t len;
};

class NetworkPacketList
{
public:
    NetworkPacketList();
    ~NetworkPacketList();

    int count();

    bool valid()
    {
        return on >= 0 && on < len;
    }

    bool next();

    void get(char **buffer, uint16_t *len);

    void reset()
    {
        on = -1;
    }

    friend class NetworkLink;

private:
    int on;
    int len;
    netif_packet_t *packets;

    // List of buffers passed in a channel to free next time this is
    // used. Not used in LocalNetworkLink.
    struct NetlinkPacket *chanpackets;
};

class NetworkLinkException
{
    const char *what;
    int num;
public:
    NetworkLinkException(const char *what_, int num_) : what(what_), num(num_)
    {
    }

    std::string getReason()
    {
        char temp[1024];

        snprintf(temp, 1024, "Error %d: %s", num, what);
        return std::string(temp);

    }
};

class NetworkLink
{

public:
    NetworkLink();
    virtual ~NetworkLink();

    // Put the mac in the given buffer
    // The buffer must be at least 6 bytes
    virtual void getmac(unsigned char *buffer) = 0;

    // Tick the network link, either this or receive must be called occasionally
    virtual void tick();
    virtual int send(char *packet, size_t len) = 0;
    virtual int receive(NetworkPacketList *packetlist, 
            struct timeval *timeout, int max_packets = FAUX_LINK_MAX_PACKETS) = 0;
    virtual int poll() = 0;

protected:

    // Extract the pointer from a NetworkPacketList
    static netif_packet_t *getPacketPointer(NetworkPacketList *packetList)
    {
        return packetList->packets;
    }

    static void setNumPackets(NetworkPacketList *packetList, int len)
    {
        packetList->len = len;
    }

    static NetlinkPacket *getChannelPackets(NetworkPacketList *packetList)
    {
        return packetList->chanpackets;
    }
};

class LocalNetworkLink : public NetworkLink
{
public:
    static int getRingCount(const char *iface)
    {
        netif_info_t info;
        netif_info(&info, iface);
        if(!info.valid)
            return -1;
        else 
            return info.rx_rings;
    }

    LocalNetworkLink(const char *iface, int ring, int numrings);
    virtual ~LocalNetworkLink();

    virtual void tick();
    virtual void getmac(unsigned char *buffer);
    virtual int send(char *packet, size_t len);
    virtual int receive(NetworkPacketList *packetlist, 
            struct timeval *timeout, int max_packets = FAUX_LINK_MAX_PACKETS);
    virtual int poll();

    unsigned int getTotalFlowGroupCount();
    void setFlowGroups(unsigned int start, unsigned int num, unsigned int skip);

private:
    netif_t *nifs;
    int num_nifs;
};

class ServerNetworkLink : public NetworkLink
{
    static const size_t packet_buf_size = 2048;

public:
    ServerNetworkLink(int id, int transport_count);
    virtual ~ServerNetworkLink();

    virtual void getmac(unsigned char *buffer);
    virtual int send(char *packet, size_t len);
    virtual int receive(NetworkPacketList *packetlist, 
            struct timeval *timeout, int max_packets = FAUX_LINK_MAX_PACKETS);
    virtual int poll();

private:
    unsigned int m_id;
    unsigned int m_transport_count;
    Endpoint *m_ep;
    Channel *m_recv_chan;
    //char m_packet_buf[packet_buf_size];
    //uint16_t m_cur_packet_len;

    Channel *m_link_chan;
};

class LoopbackNetworkLink : public NetworkLink
{
public:
    LoopbackNetworkLink(int id, int transport_count);
    virtual ~LoopbackNetworkLink();

    virtual void getmac(unsigned char *buffer);
    virtual int send(char *packet, size_t len);
    virtual int receive(NetworkPacketList *packetlist, 
            struct timeval *timeout, int max_packets = FAUX_LINK_MAX_PACKETS);
    virtual int poll();

private:
    unsigned int m_id;
    unsigned int m_transport_count;
    unsigned int m_received_packets;
    std::list<NetlinkPacket*> m_packetBuffer;
};

