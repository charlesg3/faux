#pragma once

/**
 * @file link.h
 * @brief The interface to the netlink server
 * @author Charles Gruenwald III, Nathan Beckmann
 */

//#include <messaging/messaging.h>
#include <arpa/inet.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send data to an IP address through the netlink.
 *
 * Called by the netstack.
 *
 * @param in_addr IP address to send to.
 * @param in_data Pointer to data buffer.
 * @param in_size Size in bytes of data to send.
 * @returns Status code indicated result of call.
 */
//FosStatus netLinkSend(struct in_addr * in_to, const void * in_data, size_t in_size);

/**
 * @brief Sets the master mailbox that the netlink
 * servers should use.
 *
 * Called by the netstack.
 *
 * @param in_master RemoteBox of the master netstack server
 * @returns Status code indicated result of call.
 */
//FosStatus netLinkSetMaster(const FosRemotebox * in_master);
//FosStatus netLinkSetLoMaster(const FosRemotebox * in_master);

/**
 * @brief Sets up a mapping between a particular flow id and a netstack
 *
 * Called by the netstack.
 *
 * @param in_transport RemoteBox of the transport that is responsible for this flow
 * @param in_flow_id flow id of the flow that this transport will be handling
 * @returns Status code indicated result of call.
 */
//FosStatus netLinkAddMapping(const FosRemotebox * in_transport, unsigned int in_flow_id);
//FosStatus netLinkRemoveMapping(const FosRemotebox * in_transport, unsigned int in_flow_id);

/**
 * @brief Calls the incoming processing like it was sent from the kernel
 *
 * Called by the loopback device.
 *
 * @param in_data Data to be processed
 * @param in_size Size oF data to be processed
 * @returns Status code indicated result of call.
 */
//FosStatus netLinkIncoming(const void * in_data, const size_t in_size);
//FosStatus netLinkIncomingV(struct iovec * in_iov, size_t in_iovcnt);

/**
 * @brief A MAC address.
 */
#define MAC_ADDRESS_LENGTH 6

typedef struct
{
    char data[MAC_ADDRESS_LENGTH];
} __attribute__((__packed__)) MacAddress;

/**
 * @brief Gets the mac address of the system
 *
 * Called by the netstack.
 *
 * @param out_mac Place where the mac address will be stored
 * @returns Status code indicated result of call.
 */
//FosStatus netLinkGetMac(MacAddress * out_mac);

/**
 * @brief Shuts down the link layer.
 *
 * Called by the netstack during it's shutdown procedure.
 *
 * @returns Status code indicated result of call.
 */
//FosStatus netLinkShutdown(void);

#ifdef __cplusplus
}
#endif
