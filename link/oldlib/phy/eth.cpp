#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <env/env.h>
#include <dispatch/dispatch.h>
#include <uk_syscall/syscall.h>
#include <uk_syscall/debug_timing.h>
#include <utilities/debug.h>
#include <sys/xenbus/xenbus_server.h>
#include <sys/xenbus/xenbus_interface.h>
#include <sys/net/phy/netif.h>
#include <sys/net/types.h>
#include "./include/eth.hpp"
#include "../link/link_private.h"

#include <utilities/time_arith.h>
#include <utilities/tsc.h>
#include <config/config.h>

#include <init/init.h>

using namespace fos;
using namespace messaging;
using namespace net;
using namespace phy;
using namespace status;

#ifdef CONFIG_PROF_NETSTACK
uint64_t g_link_sends = 0;
uint64_t g_link_bytes_sent = 0;
uint64_t g_link_cycles_spent = 0;
#endif

void idle(void *p);

static inline uint64_t uk_netif_init(struct netfront_dev * dev)
{
    return __do_fos_syscall_typed(SYSCALL_NETIF_INIT, dev, 0, 0, 0, 0, 0);
}

static inline uint64_t uk_netif_sync_dev(struct netfront_dev * dev)
{
    return __do_fos_syscall_typed(SYSCALL_NETIF_SYNC_DEV, dev, 0, 0, 0, 0, 0);
}

static void free_netfront(struct netfront_dev *dev)
{

    free(dev->backend);
    free(dev->nodename);
    /*
       mask_evtchn(dev->evtchn);


       gnttab_end_access(dev->ring_ref);
       free_page(dev->ring.sring);

       unbind_evtchn(dev->evtchn);
    */
    //FIXME: this should be a bit more proper
    free(dev);
}

static struct netfront_dev *init_netfront(
    FosRemotebox xenbus_server,
    ReplyEndpoint xenbus_receive,
    ReplyEndpoint uk_receive,
    char *nodename,
    uint64_t dom)
{
    xenbus_transaction_t xbt;
    const char* err;
    const char* message=NULL;
    const char* msg;
    int retry=0;

    char path[strlen(nodename) + 1 + 10 + 1];
    char out[1024] = " \033[22;32m*\033[0m    ";

    struct netfront_dev *dev = (struct netfront_dev *)malloc(sizeof(*dev));
    memset(dev, 0, sizeof(*dev));
    dev->nodename = strdup(nodename);

    dev->capability = uk_receive.capability;
    dev->alias = uk_receive.alias;
    dev->location = uk_receive.mailbox->location;

    dev->dom = dom;

    uk_netif_init(dev);

    snprintf(path, sizeof(path), "%s/backend-id", nodename);
    dev->dom = xenbus_read_integer(xenbus_server, xenbus_receive, path);

again:
    err = xenbus_transaction_start(xenbus_server, xenbus_receive, &xbt);
    if (err) {
        printf("starting transaction\n");
    }

    err = xenbus_printf(xenbus_server, xenbus_receive, xbt, nodename, "tx-ring-ref","%u",
            dev->tx_ring_ref);
    if (err) {
        message = "writing tx ring-ref";
        goto abort_transaction;
    }
    err = xenbus_printf(xenbus_server, xenbus_receive, xbt, nodename, "rx-ring-ref","%u",
            dev->rx_ring_ref);
    if (err) {
        message = "writing rx ring-ref";
        goto abort_transaction;
    }
    err = xenbus_printf(xenbus_server, xenbus_receive, xbt, nodename,
            "event-channel", "%u", dev->evtchn);
    if (err) {
        message = "writing event-channel";
        goto abort_transaction;
    }

    err = xenbus_printf(xenbus_server, xenbus_receive, xbt, nodename, "request-rx-copy", "%u", 1);

    if (err) {
        message = "writing request-rx-copy";
        goto abort_transaction;
    }

    err = xenbus_printf(xenbus_server, xenbus_receive, xbt, nodename, "state", "%u",
            4); /* connected */


    err = xenbus_transaction_end(xenbus_server, xenbus_receive, xbt, 0, &retry);
    if (retry) {
        goto again;
        printf("completing transaction\n");
    }

    goto done;

abort_transaction:
    xenbus_transaction_end(xenbus_server, xenbus_receive, xbt, 1, &retry);
    goto error;

done:

    snprintf(path, sizeof(path), "%s/backend", nodename);
    msg = xenbus_read(xenbus_server, xenbus_receive, XBT_NIL, path, &dev->backend);
    snprintf(path, sizeof(path), "%s/mac", nodename);
    msg = xenbus_read(xenbus_server, xenbus_receive, XBT_NIL, path, &dev->mac_str);

    if ((dev->backend == NULL) || (dev->mac_str == NULL)) {
        printf("%s: backend/mac failed\n", __func__);
        goto error;
    }

    printf(out);
    //printf("frontend: %s backend: %s ", nodename, dev->backend);
    printf("mac: %s\n", dev->mac_str);

    //store a numeric value of the mac address
    sscanf(dev->mac_str,"%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", \
            &(dev->mac_addr.data[0]), &(dev->mac_addr.data[1]), \
            &(dev->mac_addr.data[2]), &(dev->mac_addr.data[3]), \
            &(dev->mac_addr.data[4]), &(dev->mac_addr.data[5]));


    {
        char path[strlen(dev->backend) + 1 + 5 + 1];
        snprintf(path, sizeof(path), "%s/state", dev->backend);

        //xenbus_watch_path_token(XBT_NIL, path, path, &dev->events);

        //xenbus_wait_for_value(path, "4", &dev->events);
        xenbus_wait_for_value(xenbus_server, xenbus_receive, path, "4", NULL);

        unsigned char *ip;
        snprintf(path, sizeof(path), "%s/ip", dev->backend);
        xenbus_read(xenbus_server, xenbus_receive, XBT_NIL, path, (char **)&ip);
        if(ip)
            printf("read ip: %s\n", ip);
        free(ip);

#if 0
        snprintf(path, sizeof(path), "%s/mac", dev->backend);
        xenbus_read(xenbus_server, xenbus_receive, XBT_NIL, path, &dev->mac_str);
        printf("mac is %s\n",dev->mac_str);
#endif
    }

    uk_netif_sync_dev(dev);
    return dev;

error:
    free_netfront(dev);
    return NULL;

}

FosStatus Eth::populateDev()
{
    // Fill in info for the kernel through xenbus
    Remotebox xenbus_rbox = Remotebox::lookup(Alias("/sys/xenbus/@local"));

    //replies from xenbus
    messaging::Mailbox xenbus_mbox = messaging::Mailbox();

    ReplyEndpoint reply;
    reply.alias = xenbus_mbox.canonicalAlias().ref();
    reply.capability = xenbus_mbox.createCapability();
    reply.mailbox = xenbus_mbox.ref();

    ReplyEndpoint uk_reply;
    uk_reply.alias = m_mbox.canonicalAlias().ref();
    uk_reply.capability = m_mbox.createCapability();
    uk_reply.mailbox = m_mbox.ref();

    int dom_id = xenbus_get_self_id(xenbus_rbox.ref(), reply);

    // query xenstore about attached network interfaces
    int node_id = 0;
    char **dirs, *msg;
    char pre [1024];
    snprintf(pre, sizeof(pre), "/local/domain/%d/device/vif", dom_id);
    int x;

    msg = xenbus_ls(xenbus_rbox.ref(), reply, XBT_NIL, pre, &dirs);

    if (msg) {
        PS("Error in xenbus ls: %s", msg);
        free(msg);
        return FOS_MAILBOX_STATUS_GENERAL_ERROR;
    }
    for (x = 0; dirs[x]; x++) 
    {
        node_id = atoi(dirs[x]);
        free(dirs[x]);
    }
    free(dirs);

    char device_path [1024];
    snprintf(device_path, sizeof(device_path), "device/vif/%d", node_id);

    // get the dom
    snprintf(pre, sizeof(pre), "%s/backend-id", device_path);

    int dev_dom = xenbus_read_integer(xenbus_rbox.ref(), reply, pre);

    char my_path [1024];
    snprintf(my_path, sizeof(my_path), "device/vif/%d", node_id);

    m_dev = init_netfront(xenbus_rbox.ref(), reply, uk_reply, device_path, dev_dom);

    return FOS_MAILBOX_STATUS_OK;
}

#define TX_BUF_MAILBOX 0
#define TX_BUF_MALLOCED 1
#define TX_BUF_INVALID -1

typedef struct
{
    bool inuse;
    int type;
    void *data;
} tx_buf_ref;

static unsigned int g_tx_buf_max = 1024;
static unsigned int g_tx_buf_count = 0;
static tx_buf_ref *g_tx_bufs;

//Garbage collect TX buffers
static inline void gc_tx_bufs()
{
    unsigned int i;
    unsigned int seen_bufs = 0;
    unsigned int freed_bufs = 0;
    for(i = 0; i < g_tx_buf_max && seen_bufs < g_tx_buf_count; i++)
    {
        if(g_tx_bufs[i].inuse)
        {
            seen_bufs++;

            uint64_t *data;
            assert(g_tx_bufs[i].type == TX_BUF_MAILBOX);    //only 1 type now

            LinkSendMessage *msg = (LinkSendMessage*)g_tx_bufs[i].data;
            data = (uint64_t *)(&(msg->data));

            if(data[0] == 0x00FF00FF && data[1] == 0xFF00FF00)
            {
                freed_bufs++;
                g_tx_bufs[i].inuse = false;
                fosMailboxBufferFree((char *)(g_tx_bufs[i].data) - sizeof(DispatchMessageHeader));
                g_tx_bufs[i].type = TX_BUF_INVALID;
            }
        }
    }
    g_tx_buf_count -= freed_bufs;
}

//mark this message as being queued to send so we can free it later when done
static inline int mark_tx_msg(LinkSendMessage *msg)
{
    unsigned int i;
    for(i = 0; i < g_tx_buf_max; i++)
    {
        if(!g_tx_bufs[i].inuse)
        {
            g_tx_buf_count++;
            g_tx_bufs[i].inuse = true;

            g_tx_bufs[i].type = TX_BUF_MAILBOX;
            g_tx_bufs[i].data = msg;
            //fprintf(stderr, "In mark OK i = %d msg%lx\n", i, msg);
            
            return 0;
        }
    }
    fprintf(stderr, "In mark error no free g_tx_bufs\n");
    return -1;  //all tx_bufs in use
}

//#define DEBUG_NETSTACK 1

#if defined(DEBUG_NETSTACK)
#define uarray_size 1024

static timing_array uta[uarray_size];
static unsigned int uta_index = 0;
static unsigned int unext_entry = 0;

void get_uta(void **buff, unsigned int *index, unsigned int *asize) {
    *buff = (void *)&uta[0];
    *index = unext_entry * sizeof(timing_array);
    *asize = uarray_size * sizeof(timing_array);
    printf("Get_UTA called\n");
}

static inline unsigned long readtscll(void)
{
    unsigned int a,d;
    asm volatile("rdtsc" : "=a" (a), "=d" (d));
    unsigned long ret = d;
    ret <<= 32;
    ret |= a;
    return ret;
}
int push_uinfo(int ta_index, unsigned int event, int size, int type, int protocol) {
    //handle wrap
    if (unext_entry >= (uarray_size - 1) )
    {
        uta[unext_entry].ta_index = 99;    //be sure last entry knows it wrapped
        unext_entry = 0; //loop circular buffer
        //printf("ubuffer wrapped\n");
    }

    uta[unext_entry].timestamp = readtscll();
    uta[unext_entry].u.info.size = size;
    uta[unext_entry].u.info.type = type;
    uta[unext_entry].u.info.protocol = protocol;
    uta[unext_entry].event_type = event;
    uta[unext_entry++].ta_index = ta_index;
    //fprintf(stderr, "Push_uinfo: index=%d, event=%u next_entry=%d bindex=%lu pktsize=%d proto=%d\n", ta_index, event, unext_entry, unext_entry * sizeof(timing_array), size, protocol);
    return(ta_index);
}

int push_utiming(int ta_index, char * tag, unsigned int event) {
    //handle wrap
    if (unext_entry >= (uarray_size - sizeof(timing_array)))
    {
        uta[unext_entry].ta_index = 99;    //be sure last entry knows it wrapped
        unext_entry = 0; //loop circular buffer
        //printk("buffer wrapped\n");
    }
   
    uta[unext_entry].timestamp = readtscll();
    strncpy(uta[unext_entry].u.tag, tag, sizeof(uta[unext_entry].u.tag));
    uta[unext_entry].event_type = event;
    uta[unext_entry++].ta_index = ta_index;
    fprintf(stderr, "Push_utiming: index=%d, tag '%s' event=%u next_entry=%d bindex=%lu\n", ta_index, tag, event, unext_entry, unext_entry * sizeof(timing_array));
    return(ta_index);
}

#endif

/*
static void *out_thread(void *p)
{
    while(1)
        idle(p);

    return NULL;
}
*/

void Eth::notifyStarted()
{
    initNotifyStarted(m_out_mbox->capability());
}

// Make upcall into link layer with data
void Eth::incoming(void * msg, size_t size, void * vpeth)
{
    Eth * eth = (Eth *) vpeth;
    eth->m_link.incoming(msg, size);
}

void idle(void *p)
{
    Eth *eth_obj = (Eth *)p;
    void * received_message;
    size_t received_size;
    FosStatus receive_error;

    receive_error = fosMailboxReceive((void**) &received_message, &received_size, eth_obj->getOutMailbox()->ref());
    if ((receive_error == FOS_MAILBOX_STATUS_ALLOCATION_ERROR) || (receive_error == FOS_MAILBOX_STATUS_INVALID_MAILBOX_ERROR))
    {
        fprintf(stderr, "unrecoverable mailbox recieve error\n");
        assert(false);
    }
    else if (receive_error == FOS_MAILBOX_STATUS_EMPTY_ERROR)
    {
        return;
/*        continue; */
    }

    uint8_t *recvp = (uint8_t *)received_message;
    LinkSendMessage *msg = (LinkSendMessage *)(recvp + sizeof(DispatchMessageHeader));
    
#if defined(DEBUG_NETSTACK)

    //skip headers
    unsigned char *data = msg->data;
#if 0
    static int inited = 0;

    if (inited++ == 0)
    {
        for (i=0; i<0x100; i += 0x10)
        {
            printf("data[%d] %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u \n", i, data[i+0], data[i+1], data[i+2], data[i+3], data[i+4], data[i+5], data[i+6], data[i+7], data[i+8], data[i+9], data[i+10], data[i+11], data[i+12], \
                    data[i+13], data[i+14], data[i+15]);
        }
    }
#endif
    // do deep packet inspection to determine packet type
    struct ethernet_frame *pkt = (struct ethernet_frame *)data;
    int type = ntohs(pkt->type);
    int protocol = -1;

    if(type == 0x0800) //IP
    {
        struct ip_frame *ip = (struct ip_frame *)&data[sizeof(struct ethernet_frame)];
        protocol = ip->protocol;
    }

    push_uinfo(uta_index++, PACKET_INFO_START, msg->size, type, protocol);
#endif

    USE_PROF_NETSTACK(unsigned long long start = rdtscll();)

#ifdef CONFIG_NET_PHY_BUFFER_POOL
    __do_fos_syscall_typed(SYSCALL_NETIF_SEND, msg->data, msg->size, 0, 0, 0, 0);
    fosMailboxBufferFree(recvp);
#else
    //fprintf(stderr, "In eth.cpp about to mark message\n");
    assert(mark_tx_msg(msg) == 0);
    //fprintf(stderr, "In eth.cpp about to send message\n");
    __do_fos_syscall_typed(SYSCALL_NETIF_SEND, msg->data, msg->size, 0, 0, 0, 0);
    //fprintf(stderr, "In eth.cpp sent message\n");
    gc_tx_bufs();
    //fprintf(stderr, "In eth.cpp ret from gc\n");
    #endif

#ifdef CONFIG_PROF_NETSTACK
    g_link_sends++;
    g_link_bytes_sent += msg->size;
    g_link_cycles_spent += rdtscll() - start;
#endif
}

    Eth::Eth(Link & link)
: Phy(link)
    , m_mbox("/sys/net/@local/if/eth0")
    , m_out_mbox(0)
{
    // Initialize ourselves with uk
    check(
            populateDev());
    
/*    m_out_mbox = new Mailbox("/sys/net/@local/if/eth0_out", FOS_MAX_MESSAGE_SIZE);
		 FOS_MAILBOX_OPTIONS_URPC_2,
                   FOS_FLAG_NONE, FOS_FLAG_NONE,
		 (void *)__do_fos_syscall_typed(SYSCALL_ALLOC_CONTIG_BUFFER, FOS_MAX_MESSAGE_SIZE, getDevId(), 0 , 0 , 0, 0));
*/
    m_out_mbox = new Mailbox("/sys/net/@local/if/eth0_out");
    //Initialize our tx_bufs
    g_tx_bufs = (tx_buf_ref *)malloc(g_tx_buf_max * sizeof(tx_buf_ref));
    unsigned int  i;
    
    for(i = 0; i < g_tx_buf_max; i++)
    {
        g_tx_bufs[i].inuse = false;
        g_tx_bufs[i].type = TX_BUF_INVALID;
    }
    g_tx_buf_count = 0;
    
    // Insert callback for data from uk
    dispatchRegisterRawMailbox(
        m_mbox.ref(),
        incoming,
        this);

    dispatchRegisterIdleHandler(idle, this);
    //pthread_create(&m_outgoing_thread, NULL, out_thread, this);
}

Eth::~Eth()
{
    m_mbox.unreg(Alias("/sys/net/@local/if/eth0"));
    m_mbox.unreg(Alias("/sys/net/@local/if/eth0_out"));
}

FosStatus Eth::send(void * in_data, size_t in_size)
{
#if defined(DEBUG)
    struct ethernet_frame *eth = (struct ethernet_frame *)in_data;
    printf("from: %hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
            eth->source[0], 
            eth->source[1], 
            eth->source[2], 
            eth->source[3], 
            eth->source[4], 
            eth->source[5]);
    printf(" to: %hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
            eth->dest[0], 
            eth->dest[1], 
            eth->dest[2], 
            eth->dest[3], 
            eth->dest[4], 
            eth->dest[5]);
    printf(" type: %hhx%hhx", ((char *)&eth->type)[0], ((char *)&eth->type)[1]);

    struct ip_frame *ip = (struct ip_frame *)&(((char *)in_data)[sizeof(struct ethernet_frame)]);
    printf("ip protocol: %d\n", ip->protocol);


    if(ip->protocol == 17)
    {
        unsigned int header_length = ip->version & 0xF;
        struct udp_frame *udp = (struct udp_frame *)&((char *)in_data)[sizeof(struct ethernet_frame) + (header_length * 4)];
        printf(" src port: %d dest port: %d\n", ntohs(udp->source_port), ntohs(udp->dest_port));
    }
#endif

    assert(false);

    __do_fos_syscall_typed(SYSCALL_NETIF_SEND, in_data, in_size, 0, 0, 0, 0);

    return FOS_MAILBOX_STATUS_OK;
}

void Eth::pause()
{
    dispatchDisableMailbox(m_mbox.ref());
}

void Eth::unpause()
{
    dispatchEnableMailbox(m_mbox.ref());
}

MacAddress Eth::getMacAddress() const
{
    return m_dev->mac_addr;
}

struct netfront_dev *Eth::getDevId() const
{
    return m_dev;
}

void Eth::quit()
{
#ifdef CONFIG_PROF_NETSTACK
    uint64_t ms_spent = ts_to_ms(g_link_cycles_spent);

    fprintf(stderr, " Link (eth): %ld writes, %ld bytes, %ld cycles, %ld ms (%ld kb/s)\n", 
            g_link_sends, 
            g_link_bytes_sent,
            g_link_cycles_spent,
            ms_spent,
            ms_spent ? g_link_bytes_sent / ms_spent : 0);

#endif
}

