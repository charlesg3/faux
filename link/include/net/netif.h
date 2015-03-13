#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <net/if.h>
#include <inttypes.h>
#include <string.h>
#include <netmap_new/netmap.h>


#define EMAPFAILED 0x1000
#define ESYNCFAILED 0x1001
#define EFDERROR 0x1002
#define EBUG 0x1003
#define EIFDOWN 0x1004
#define EBADIF 0x1005

#ifdef CONFIG_NETIF_DEBUG
#define DEBUG(fmt, ...) \
    fprintf(stderr, "debug %s:%d: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#else
    // Actually include values so they appear to be "used" and
    // we don't get warnings
#define DEBUG(fmt, ...) \
    fmt; __VA_ARGS__;
#endif

#define NETIF_NUM_FLOW_GROUPS 4096


typedef struct 
{
    int fd;        // fd for recving and sending
    char *shm;      // netmap shared memory region
    struct netmap_if *nif; // netmap interface
    int ring;

    char name[16];  // Name of the device, truncated to 15 chars
} netif_t;

typedef struct
{
    int valid;
    uint32_t tx_rings;
    uint32_t tx_slots;

    uint32_t rx_rings;
    uint32_t rx_slots;
} netif_info_t;



/*
 * Get information about an interface
 *
 * Puts information about the named interface into the
 * given netif_info_t struct.
 * The information lists details about the rings of
 * the interface.
 *
 * If the interface is invalid, out_info->valid is 
 * 0.
 */
void netif_info(netif_info_t *out_info,const char *iname);


/*
 * Register for packet transmission/send on a specific 
 * interface name (e.g. "eth1").
 * This returns 0 on success, or an error value on error, 
 * and stores network interface info in the struct 
 * pointed to by out_if.
 * Ring num is either a hardware ring number or a 
 * macro indicating all rings (probably NET_ALL_RINGS). 
 */
int netif_init(netif_t *out_if,const char *iname, int ringnum);


/*
 * Close the previously opened netif
 */
int netif_close(netif_t *in_if);


/*
 * Set the interfaces to do all processing on the given
 * cpu number.
 *
 * This also sets process affinity to only that cpu
 */
int netif_bind_cpu(netif_t *in_ifs, size_t num_ifs, int cpu);

/*
 * Set up flow groups for a set of interfaces
 *
 * There are 4096 flow groups each corresponding to the low
 * 12 bits of the source port for TCP and UDP.
 *
 * This function circularly assigns 'num' flows to the given
 * netifs starting with flow 'start'.
 *
 * This function can skip a certain number of flow group at each assignment
 * using the skip parameter.
 *
 * For example:
 * in_ifs = {a (ring 0), b (ring 1), c (ring 2)}; start = 0; num = 5; skip = 1
 *      flows 0,3 => ring 0
 *      flows 1,4 => ring 1
 *      flow    2 => ring 2
 *
 * start = 0; num = 5; skip = 2
 *      flows 0,6 => ring 0
 *      flows 2,8 => ring 1
 *      flow    4 => ring 2
 *
 * Use skip to distribute sequential flows across separate sets of netif_t.
 * This allows for load balancing between sequential source port numbers
 * across different ring sets.
 */
int netif_set_flow_groups(netif_t in_ifs[], size_t num_fds, uint16_t start, 
        uint16_t num, uint16_t skip);


/*
 * Return the number of packets waiting in the userspace TX
 * ring to be sent.
 *
 * This information may be used to determine when to
 * synchronize the transmit ring.
 */
int netif_querytx(netif_t *in_if);

/*
 * Explicitly synchronize the tx rings of the interfaces.
 */
int netif_synctx(netif_t *in_if);


/*
 * Send data on the opened interface
 *
 * Returns 0 on success, error code otherwise
 */
int netif_send(netif_t *in_if, char *data, size_t len);


/*
 * Send data on the opened interface, without syncing.
 *
 * You must either call netif_poll, netif_select, or netif_send
 * on this interface to actually send the data.
 */
int netif_quicksend(netif_t *in_if, char *data, size_t len);

/*
 * Poll interface for activity
 *
 * Returns number of waiting packets on success (0 if timeout),
 * or negative value on error.
 *
 * Note: The function may return -EINTR if it is interrupted
 * (this frequently happens when profiling)
 */
int netif_poll(netif_t *in_if, int timeout);

/*
 * Check several interfaces for waiting packets
 *
 * This will call select on the receive fds for the given input
 * interfaces. When there are packets waiting, the set of 'netif_t's
 * that have waiting packets will be pointed to by the elements of
 * the array pointed to by 'out_ifs' with corresponding number of 
 * waiting packets in the elemnts of the array pointed to by 'out_nums'.
 * The return value will be the number of valid elements in 'out_ifs' and
 * 'out_nums'.
 * Note that the maximum length of both in_ifs and out_ifs must be equal
 * to num_ifs.
 * 
 *
 * in_ifs     - Input array of 'num_fds' interfaces to check for packets
 * out_ifs    - Output array of pointers to interfaces with waiting packets
 * out_nums   - Output array of waiting packet count for corresponding 'out_ifs'
 * num_ifs    - Input length of in_ifs and out_ifs
 * timeout    - Input timeout value. May be NULL.
 *
 * Returns: Number of interfaces with waiting packets on success, 
 * negative error value on failure.
 */
int netif_select(netif_t in_ifs[], netif_t *out_ifs[], int out_nums[], 
        int num_ifs, struct timeval *timeout);

// Refer to a packet buffer
typedef struct
{
    char *data;     // Pointer to packet data
    uint16_t *len;  // Pointer to packet length
} netif_packet_t;


/* 
 * Receive packets from the interface
 *
 * "packets" is an array of "max_packets" structs of type netif_packet_t.
 * The buffers returned by this will only be valid until the next call
 * to either netif_poll or netif_recv. This also means that the
 * buffers will not be freed for new packets, so one of those functions
 * should be called immediately after processing.
 */
int netif_recv(netif_t *in_if, netif_packet_t packets[], 
        size_t max_packets);


/*
 * Get several buffers in which packets may be constructed for immediate
 * transmission with a following netif_send_transmit_buffers.
 *
 * Returns number of valid buffers on success, error code on failure.
 */
 
int netif_get_transmit_buffers(netif_t *in_if, netif_packet_t packets[],
        size_t num_packets);

/*
 * Get and set the MAC address for the NIC.
 * the IOCTL to the NIC for this is removed in this NetMap driver.
 *
 * Returns 0 on success, error code on failure
 */
int netif_set_mac(netif_t *in_if, unsigned char *macaddress);

int netif_get_mac(netif_t *in_if, unsigned char *macaddress);

/*
 * Perform a quick poll on the given interfaces
 *
 * If supported, this checks a userspace flag 
 * to find waiting packets on the interfaces.
 *
 * in_if is an array of interfaces to poll
 * count is the number of elements in that array
 *
 * Return the number of waiting packets
 */
int netif_quickpoll(netif_t in_ifs[], size_t count);


#ifdef __cplusplus
}
#endif

