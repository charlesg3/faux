
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/net/link.h>
#include <sys/net/types.h>
#include "../common/include/hash.h"
#include "./link_private.h"
#include "sys/net/phy/netif.h"
#include <config/config.h>
#include <utilities/debug.h>

#include <tr1/unordered_map>
#include <list>
#include <messaging/messaging.hpp>
#include <threading/ticket_lock.hpp>
#include "../phy/include/phy.hpp"
#include "../phy/include/eth.hpp"
#include "../transport/transport_private.hpp"

using namespace fos::net::transport;
using std::tr1::unordered_map;
using std::list;

using fos::Status;
using fos::net::Phy;
using fos::net::phy::Eth;
using fos::messaging::Mailbox;
using fos::messaging::Remotebox;
using fos::messaging::Alias;
using fos::threading::TicketLock;
using namespace fos::status;

typedef unordered_map<flow_t, TicketLock> FlowLockMap;

// Static data used for lib
FlowLockMap m_flow_lock_map;

static FosStatus forward_data(flow_t hash, void * data, size_t len, FosRemotebox * master_netstack);

static void computeFlowId(char * out_buffer, flow_t hash)
{
    sprintf(out_buffer, "/sys/net/@local/flow/%x", hash);
}

void link_handle_incoming(void * vpdata, size_t len, FosRemotebox *master_netstack)
{
    uint8_t * data = (uint8_t *) vpdata;


    struct ethernet_frame *p = (struct ethernet_frame *)data;

    // do deep packet inspection to determine if we should forward this packet
    // to a registered flow
    int type = ntohs(p->type);
    if(type == 0x0800) //IP
    {
        struct ip_frame *ip = (struct ip_frame *)&data[sizeof(struct ethernet_frame)];

        //BMK put a sanity check here for a valid packet before forwarding
        if ( (ip->version & 0xf0)  != 0x40 ) {
            printf("Bad packet in linkserver bad IP version %x\n", ip->version);
            return; //dump invalid packet
        }
        if(ip->protocol == 1) //icmp
        {
        }
        else if(ip->protocol == 6) //tcp
        {
            unsigned int header_length = ip->version & 0xF;
            struct tcp_frame *t = (struct tcp_frame *)&data[sizeof(struct ethernet_frame) + (header_length * 4)];

            in_addr_t source;
            in_addr_t dest;
            source = ip->source;
            dest = ip->dest;

            flow_t hash;
            uint8_t flags = ((uint8_t *)&t->control)[1]; // fix the byte order of the control field

            if ((flags & TCP_SYN) && ! (flags & TCP_ACK)) // incoming packet trying to esablish an incoming connection to a listening socket
            {
                hash = JSNetHash(dest, ntohs(t->dest_port), 0, 0);

                // ignore return val intentionally, don't want to spam listening socket -nzb
                forward_data(hash, data, len, master_netstack);
            }
            else
            {
                hash = JSNetHash(dest, ntohs(t->dest_port), source, ntohs(t->source_port));

                // If we can't hit this alias, then retry the
                // listening socket if this might be the last ACK of
                // the three-way handshake
                if (forward_data(hash, data, len, master_netstack) == FOS_MAILBOX_STATUS_INVALID_ALIAS_ERROR
                    && (flags & TCP_ACK))
                {
                    hash = JSNetHash(dest, ntohs(t->dest_port), 0, 0);
                    forward_data(hash, data, len, master_netstack);
                }
            }

            return;
        } // tcp
    } // ip

    else if(type == 0x0806) //broadcast arp replies to that n-distinct netstacks can keep their tables updated
    {
        // We iterator over all netstacks by trying to send until one
        // hits an invalid alias error.
        // @todo There could be gaps in this list once we do
        // elasticity.
        for(int j = 0; ; j++)
        {
            char ns_str[128];
            snprintf(ns_str, sizeof(ns_str), "/sys/net/@local/transport/if/%d", j);

            Remotebox ns(Alias(ns_str), master_netstack->capability);

#if 0 // exceptions do not work with cooperative threads
            try {
                ns.sendData(data, len);
            } catch (fos::status::Exception & e) {
                assert(e.status() == FOS_MAILBOX_STATUS_INVALID_ALIAS_ERROR);
                return;
            }
#else
            FosStatus s;
            switch (s = fosMailboxSendBlocking(&ns.alias(), ns.capability(), data, len))
            {
            case FOS_MAILBOX_STATUS_OK:
                break;

            case FOS_MAILBOX_STATUS_INVALID_ALIAS_ERROR:
                return;

            default:
                PS("Unexpected messaging error to '%s' : %d", ns.alias().data, s);
                assert(false);
            }
#endif
        }
        return;
    }

    // fall through for various cases
    FosStatus s;
    switch (s = fosMailboxSendBlocking(&master_netstack->alias, master_netstack->capability, data, len))
    {
    case FOS_MAILBOX_STATUS_OK:
        break;

    case FOS_MAILBOX_STATUS_INVALID_ALIAS_ERROR:
        return;

    default:
        PS("Unexpected messaging error to '%s' : %d", master_netstack->alias.data, s);
        assert(false);
    }
//    master_netstack.sendData(data, len);
}

static FosStatus forward_data(flow_t hash, void * data, size_t len, FosRemotebox * master_netstack)
{
    Status ret = FOS_MAILBOX_STATUS_OK;

    // In order to avoid re-ordering in the messaging layer,
    // we now ensure that messages for a single flow are
    // delivered in order. This is done by keeping a list of
    // threads for each flow, and making sure that the head of
    // that list is always active. The other threads will go
    // to sleep until all the previous sends have completed.
    //TicketLock & tl = m_flow_lock_map [ hash ];
    //tl.acquire();

    // Compute the alias for this flow
    char flow_id[128];
    computeFlowId(flow_id, hash);

    Alias flow_alias = Alias(flow_id);

    Remotebox netstack_fleet_rbox(flow_alias, master_netstack->capability);

    // try to send to this flow
    if (ret == FOS_MAILBOX_STATUS_OK) {
        ret = netstack_fleet_rbox.sendData(data, len, false);
    }

again:
    switch (ret)
    {
    case FOS_MAILBOX_STATUS_OK:
        break;

    case FOS_MAILBOX_STATUS_INVALID_ALIAS_ERROR:
        // flow hasn't been established, this is a tcp packet to
        // an unknown destination

        // do nothing = drop packet
        //   this happens for every connection, not sure why, but everything works so I'm eliminating spam -nzb
        // PS("Hash not set up for TCP -- dropping packet! hash : %s", flow_id);
        break;

        // If the mailbox is full, just drop the packet.
    case FOS_MAILBOX_STATUS_NO_SPACE_ERROR:
        PS("Transport mailbox full, resending! hash : %s", flow_id);
        ret = netstack_fleet_rbox.sendData(data, len, false);
        goto again;
        /// @todo We are dropping the packet. We should count this in stats somewhere.
        break;


    case FOS_MAILBOX_STATUS_PERMISSIONS_ERROR:
        PS("Transport doesn't have permission to deliver packet for flos: %s", flow_id);
        break;

    default:
        PS("Unexpected error (%d) forwarding data to hash : %s", ret, flow_id);
        // Some unknown fatal error ... drop packet
        break;
    }

    // pull ourselves off thread list and wake the next member (or
    // clean up map if its empty)
    /*
    if (!tl.release())
    {
        // @todo Is this a good idea? Should we periodically
        // clean up rather than after every packet?
        m_flow_lock_map.erase(hash);
    }
    */

    return ret;
}


