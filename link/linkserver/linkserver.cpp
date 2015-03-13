#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "messaging/dispatch.h"
#include <messaging/channel.h>
#include <name/name.h>
#include "common/util.h"

#include <sys/net/types.h>
#include <net/netif.h>
#include <net/fauxlink.h>
#include <sys/net/faux_lwip.h>
#include <sys/net/link.h>
#include "linkserver_private.h"

//Globals
LocalNetworkLink *g_linkserver;

Channel **g_transport_chans = NULL;
Endpoint *g_ep;
int g_transport_count = 0;
int g_id;

void send_handler(Channel * from, DispatchTransaction trans, void * message, size_t size)
{
    //void * reply = dispatchAllocate(from, size);
    int ret = g_linkserver->send((char*)message, size);
    if(ret < 0)
    {
        fprintf(stderr, "Linkserv: send packet failed\nReturned %d\n", -ret);
    }
    /* we don't send an ack message because that would create unnecessary message traffic
     * If the packet didn't get sent the TCP stack will fix it. UDP and Broadscast not so sure */
    dispatchFree(trans, message);
}

void get_mac_handler(Channel * from, DispatchTransaction trans, void * message, size_t size)
{
    MacAddress mac;
    void * reply = dispatchAllocate(from, sizeof(mac));
    try
    {
        g_linkserver->getmac((unsigned char *)&mac);
        memcpy(reply, (void *)&mac, sizeof(mac));
    }
    catch(NetworkLinkException e)
    {
        memset(reply, 0, sizeof(mac));    //Error case return 0
    }

    dispatchRespond(from, trans, reply, sizeof(mac));
    dispatchFree(trans, message);
}

// Called when a transport server wishes to register with the link server
void register_transport_handler(Channel * from, DispatchTransaction trans, void * message, size_t size)
{
    LinkRegisterTransportRequest *req;
    LinkRegisterTransportResponse *resp;

    req = (LinkRegisterTransportRequest *)message;

    // Retrieve info out of the message
    Address transport_addr = req->ep_addr;
    int id = req->id;

    // Respond to the caller
    resp = (LinkRegisterTransportResponse *)dispatchAllocate(from, sizeof(LinkRegisterTransportResponse));
    resp->ack = 1;
    dispatchRespond(from, trans, resp, sizeof(LinkRegisterTransportResponse));
    dispatchFree(trans, message);

    fprintf(stderr, "[%d] registered id: %d as addr: %lx\n", g_id, id, transport_addr);

    // Create the channel and store it in the transport_chans
    g_transport_chans[id] = (Channel *)endpoint_connect(g_ep, transport_addr);
}

// Called when a transport server wishes to un-register with the link server
void unregister_transport_handler(Channel * from, DispatchTransaction trans, void * message, size_t size)
{
    LinkUnregisterTransportRequest *req = (LinkUnregisterTransportRequest *)message;
    LinkUnregisterTransportResponse *resp;
    int id = req->id;

    // Send response to caller
    resp = (LinkUnregisterTransportResponse *)dispatchAllocate(from, sizeof(LinkUnregisterTransportResponse));
    resp->ack = 1;
    dispatchRespond(from, trans, resp, sizeof( LinkUnregisterTransportResponse));
    dispatchFree(trans, message);

    // Remove the channel from our list
    channel_close(g_transport_chans[id]);
    g_transport_chans[id] = NULL;
}

void quit_handler(Channel * from, DispatchTransaction trans, void * message, size_t size)
{
    dispatchFree(trans, message);
    dispatchStop();
}

void idle(void *);

int main(int argc, char *argv[])
{
    Address linkserver_ep_addr;

    if(argc < 4)
    {
        fprintf(stderr, "Linkserver usage:  %s <transport_count> <linkserver_count> <ethx>\n", argv[0]);
        return 1;
    }

    // Extract values from args
    g_transport_count = atoi(argv[1]);
    int linkserver_count = atoi(argv[2]);
    char *if_name = argv[3];
    int rings = LocalNetworkLink::getRingCount(if_name);
    if(rings == -1)
    {
        printf("Could not read ring count\n");
        exit(1);
    }

    pid_t childpid;
    int status;

    for(int i = 1; i < linkserver_count; i++)
    {
        childpid = fork();
        if (childpid == -1)
        {
            perror("Cannot proceed. fork() error");
            exit(-1);
        }

        if (childpid == 0) // child
        {
            g_id = i;
            goto finish_startup;
        }
    }

    g_id = 0; // Master init

finish_startup:

    int ring_block_size = rings / linkserver_count;
    int ring_start = g_id * ring_block_size;
    assert(ring_start + ring_block_size <= rings);

    // Create NetworkLink object
    try
    {
        printf("try to open %s\n", if_name);
        g_linkserver = new LocalNetworkLink(if_name, ring_start, ring_block_size);
    }
    catch(NetworkLinkException *e)
    {
        printf("Failed to open network link (%s) [%d,%d]:\n%s\n", if_name, ring_block_size, rings, e->getReason().c_str());
        return 1;
    }

    g_transport_chans = (Channel **)malloc(sizeof(Channel *) * g_transport_count);
    g_ep = endpoint_create();

    // Name Register
    linkserver_ep_addr = endpoint_get_address(g_ep);
    name_init();
    name_register(LINKSERVER_NAME, linkserver_ep_addr);
    name_register(LINKSERVER_NAME + g_id + 1, linkserver_ep_addr);

    // Dispatch Setup
    dispatchListen(g_ep);
    dispatchRegister(LINK_REGISTER_TRANSPORT, register_transport_handler);
    dispatchRegister(LINK_UNREGISTER_TRANSPORT, unregister_transport_handler);
    dispatchRegister(LINK_SEND_PACKET, send_handler);
    dispatchRegister(LINK_GET_MAC_ADDRESS, get_mac_handler);
    dispatchRegister(LINK_QUIT, quit_handler);

    dispatchRegisterIdle(idle, NULL);

    printf("linkserver: started\n");
    fflush(stdout);

    /// Passes control on to the dispatcher
    dispatchStart();

    dispatchUnregister(LINK_SEND_PACKET);
    dispatchUnregister(LINK_GET_MAC_ADDRESS);
    dispatchUnregister(LINK_REGISTER_TRANSPORT);
    dispatchUnregister(LINK_UNREGISTER_TRANSPORT);
    dispatchUnregister(LINK_QUIT);
    dispatchIgnore(g_ep);

    delete g_linkserver;
}

NetworkPacketList packetlist;

static unsigned int route_to_transport_id(char *data, uint16_t len)
{
    struct ethernet_frame *eth = (struct ethernet_frame *)data;
    int type = ntohs(eth->type);
    if(type == 0x0800) // ip
    {
        struct ip_frame *ip = (struct ip_frame *)&((data)[sizeof(struct ethernet_frame)]);

        if(ip->protocol == 6) // tcp
        {
            unsigned int header_len = ip->version & 0xF;
            struct tcp_frame *tf = (struct tcp_frame *) &data[sizeof(struct ethernet_frame) + (header_len * 4)];

            //in_addr_t source = ip->source;
            //in_addr_t dest = ip->dest;

            uint8_t flags = ((uint8_t *) &tf->control)[1];

            if((flags & TCP_SYN) && !(flags & TCP_ACK)) // SYN packet
            {
                //For now we spread incoming connection requests to 
                //same as established conneciton requests
                return ntohs(tf->source_port) % g_transport_count;
            }
            else
            {
                return ntohs(tf->source_port) % g_transport_count;
            }
        }
        else if(ip->protocol == 17)
        {
            unsigned int header_length = ip->version & 0xF;
            struct udp_frame *udp = (struct udp_frame *)&((char *)data)[sizeof(struct ethernet_frame) + (header_length * 4)];
            if(ntohs(udp->source_port) == 67 || ntohs(udp->source_port) == 68)
                return 0;
            return ntohs(udp->source_port) % g_transport_count;
        }
        else
            return 0;
    }
    else if(type == 0x0806)
    {
        return 0;
    }
    return 0;
}

void idle(void * ignored)
{
    // get next received message from ring
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if(!packetlist.valid())
        g_linkserver->receive(&packetlist, &timeout);

    while (packetlist.next())
    {
        char *buf;
        uint16_t len;

        packetlist.get(&buf, &len);

        int chosen_id = route_to_transport_id(buf, len);

        Channel *forward_chan = g_transport_chans[chosen_id];

        if(forward_chan)
        {
            char *remote;
            remote = (char *)channel_allocate(forward_chan, len);
            if(remote == NULL)
            {
                return;
            }

            memcpy(remote, buf, len);
            channel_send(forward_chan, remote, len);
        }
    }
}

