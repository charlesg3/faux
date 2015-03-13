#include <tr1/unordered_map>
#include <list>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <threading/sched.h>
#include <dispatch/dispatch.h>
#include <messaging/messaging.hpp>
#include <sys/net/link.h>
#include <sys/net/types.h>
#include <utilities/debug.h>
#include <threading/ticket_lock.hpp>
#include <init/init.h>
#include <init/reinit.h>
#include <env/env.h>
#include <config/config.h>
//#include <pms_mm.h>
#include <uk_syscall/syscall_numbers.h>
#include <uk_syscall/syscall.h>
#include <uk_syscall/debug_timing.h>

#include <utilities/time_arith.h>
#include <utilities/tsc.h>

#include "./link_private.h"
#include "../phy/include/phy.hpp"
#include "../phy/include/eth.hpp"
#include "../common/include/hash.h"
#include "../transport/transport_private.hpp"
#include "sys/net/phy/netif.h"

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

#ifdef CONFIG_PROF_NETSTACK
static uint64_t g_link_lo_sends = 0;
static uint64_t g_link_lo_bytes_sent = 0;
static uint64_t g_link_lo_cycles_spent = 0;
#endif


/* Application container class */
class Link : public fos::net::Link
{
public:

    Link();
    ~Link();

    static Link & link() { return *m_link; }

    void go();
    void quit();
    static void shutdown();

    /* handler and message types */
    void send(void * data, size_t size);
    void setMaster(const FosRemotebox & master);
    MacAddress getMacAddress();
    void incoming(void * data, size_t size);

private:
    // Required for bootstrapping
    static FosStatus forwardBootstrapInfo(void ** out_info, size_t * out_size, void * arg);
    static FosStatus bootstrap(const void * msg, const size_t size, void * arg);

    static const int LINK_THREAD_THRESHHOLD = 32;

    void init_mbox();
    void init_devices();

    void check_back_pressure();

    bool m_paused;

    static Link * m_link;
    bool m_has_master;
    Remotebox m_master_netstack;

    Mailbox * m_public_mbox;


    Phy::Vector m_phys;
};

// Singleton Instance
Link * Link::m_link;

//#define DEBUG_NETSTACK 1

#if defined(DEBUG_NETSTACK)
#define uarray_size 1024

extern void get_uta(void **buff, unsigned int *index, unsigned int *asize);

uint64_t do_dump_uta(void *buff, unsigned int size, unsigned int *ta_index)
{
void *tmp_buff = 0;
unsigned int tmp_ta_index = 0;
unsigned int tmp_ta_size = 0;
unsigned int bottomsize = 0;

    get_uta(&tmp_buff, &tmp_ta_index, &tmp_ta_size);
    //fprintf(stderr, "in do_dump_uta buffptr=%p index %d size %d\n", tmp_buff, tmp_ta_index, tmp_ta_size);

    //Copy the result into user space's buffer reorder to fit passed in buffer
    if (tmp_buff)
    {
        if (tmp_ta_index >= size)
        {
            //partial buffer before index fills user buffer
            //printf("it fits just copy over size= %d\n", size);
            memcpy(buff, (void *)(((char *)tmp_buff) + (tmp_ta_index - size)), size);
        }
        else
        {
            if (tmp_ta_index <= size)
            {
                //partial buffer before index < user buffer
                //printf("partial ta_index %d size = %d\n", tmp_ta_index, size);
                memcpy(buff, tmp_buff, tmp_ta_index);
            }
            else
            {
                if ((*(int *)((void *)(((char *)tmp_buff) + tmp_ta_index))) != 0)
                {
                    //fifo wrapped copy bottom half of fifo to start of output
                    //limit to size that will fit in userbuffer
                    //printf("wrapped\n");
                    bottomsize = (tmp_ta_size - tmp_ta_index) - (size - tmp_ta_index);
                    memcpy(buff, (void *)(((char *)tmp_buff) + (tmp_ta_size - bottomsize)), bottomsize);
                }
                //copy to half of fifo to output
                //printf("copy full top half of fifo over size = %d\n", tmp_ta_index);
                memcpy((void *)(((char *)buff) + bottomsize), tmp_buff, tmp_ta_index);
                tmp_ta_index = size;
            }
        }
    }

    if(tmp_ta_index)
    {
        *ta_index = tmp_ta_index;
    }
    return(0);
}    
#endif

// These are essentially hand-generated RPC routines. They are loose
// wrappers that extract arguments from the message, fill in the
// response message, and send it out via the dispatch library. -nzb
static void send(void * in_data, size_t size, DispatchToken token, void * arg)
{
    uint8_t *pdata = (uint8_t *)in_data;
    LinkSendMessage * msg;

    assert(size >= LINK_SEND_MESSAGE_HDR_SIZE);

    msg = (LinkSendMessage *)(pdata + sizeof(DispatchMessageHeader));
    assert(size == msg->size + LINK_SEND_MESSAGE_HDR_SIZE);

    Link::link().send(msg->data, msg->size);

    // lib does not wait for response
}

static void setMaster(void * data, size_t size, DispatchToken token, void *)
{
    LinkSetMasterMessage * msg;
    LinkSetMasterResponse response;

    assert(size == sizeof(*msg));
    msg = (LinkSetMasterMessage *) data;

    Link::link().setMaster(msg->master);
    response.status = FOS_MAILBOX_STATUS_OK;

    dispatchSendResponse(&msg->reply.alias, msg->reply.capability, &response, sizeof(response), token);
}

static void shutdown(void * data, size_t size, DispatchToken token, void *)
{
    LinkShutdownMessage * msg;
    LinkShutdownResponse response;

    assert(size == sizeof(*msg));
    msg = (LinkShutdownMessage *) data;

    Link::shutdown();

    response.status = FOS_MAILBOX_STATUS_OK;

    dispatchSendResponse(&msg->reply.alias, msg->reply.capability, &response, sizeof(response), token);
}

static void getMacAddress(void * data, size_t size, DispatchToken token, void *)
{
    LinkGetMacMessage * msg;
    LinkGetMacResponse response;

    assert(size == sizeof(*msg));
    msg = (LinkGetMacMessage *) data;

    response.mac = Link::link().getMacAddress();
    response.status = FOS_MAILBOX_STATUS_OK;

    dispatchSendResponse(&msg->reply.alias, msg->reply.capability, &response, sizeof(response), token);
}

static void incomingDispatch(void * in_data, size_t size, DispatchToken token, void *)
{
    USE_PROF_NETSTACK(unsigned long long start = rdtscll();)
    uint8_t *data = (uint8_t *)in_data;
    LinkIncomingMessage * msg;

    assert(size >= sizeof(*msg));
    msg = (LinkIncomingMessage *) (data + sizeof(DispatchMessageHeader));
    assert(size == msg->size + sizeof(*msg) + sizeof(DispatchMessageHeader));

    Link::link().incoming(msg->data, msg->size);

#ifdef CONFIG_PROF_NETSTACK
    g_link_lo_sends++;
    g_link_lo_bytes_sent += size;
    g_link_lo_cycles_spent += rdtscll() - start;
#endif

}


static const DispatchMessageType handler_types[] = {
    NET_LINK_INCOMING,
    NET_LINK_SEND,
    NET_LINK_SET_MASTER,
    NET_LINK_GET_MAC_ADDRESS,
    NET_LINK_SHUTDOWN
};

static const DispatchMessageHandler handlers[] = {
    incomingDispatch,
    send,
    setMaster,
    getMacAddress,
    shutdown
};

// Link class definition
Link::Link()
  : m_paused(false)
  , m_has_master(false)
  , m_public_mbox(0)
{
    assert(!m_link);
    m_link = this;

    //init_mbox();
    init_devices();

    init_mbox();
    
    Remotebox rbox(m_public_mbox->remotebox());

    /* register reinit handler */
    check(
        initReinitService("/sys/net/link", &rbox.ref(), m_public_mbox->ref(), forwardBootstrapInfo, this, bootstrap, this));

    // Send our capability back to init
    initNotifyStarted(m_public_mbox->capability());

    m_phys[0]->notifyStarted();

    FosMailboxAliasCapability net_alias_cap;
    check(
        fosEnvLookupMailboxAliasCapabilityStr(NULL, &net_alias_cap, fosEnv(), "/sys/net/@local/*"));
    check(
        initSend(&net_alias_cap, sizeof(net_alias_cap)));
    
}

/* Called on parent to forward info to child */
FosStatus Link::forwardBootstrapInfo(void ** out_info, size_t * out_size, void * arg)
{
    Link *t = (Link *)arg;
    Mailbox::Capability *cap = (Mailbox::Capability *)malloc(sizeof(Mailbox::Capability));
    *cap = t->m_public_mbox->capability();
    *out_info = cap;
    *out_size = sizeof(*cap);

    // @todo: Also forward alias capability for /sys/net/STAR and
    // /sys/net/@local/STAR to local instances

    return FOS_MAILBOX_STATUS_OK;
}

FosStatus Link::bootstrap(const void * msg, const size_t size, void * arg)
{
    Link *t = (Link *)arg;

    Mailbox::Capability *cap = (Mailbox::Capability *)msg;
    t->m_public_mbox->addCapability(*cap);

    return FOS_MAILBOX_STATUS_OK;
}

void Link::init_mbox()
{
    // First claim the namespace for the local aliases; important to
    // avoid alias conflicts in multi-machine boot -nzb
    Alias net_domain("/sys/net/@local/*");
    check(
        net_domain.reserve());

    unsigned long size = (64)*(1024)*1; /* size */
    struct netfront_dev *dev = ((Eth *)m_phys[0])->getDevId();

    //void *buffer = (void *) __do_fos_syscall_typed(SYSCALL_ALLOC_CONTIG_BUFFER, size, ((Eth *)m_phys[0])->getDevId(), 0 , 0 , 0, 0);
    //fprintf(stderr, "in Link initmbx: dev = %lx, cbuffer useraddress=%lx\n", dev, buffer);
    
    bool copy = true;

    m_public_mbox = new Mailbox("/sys/net/@local/link" /* alias */,
            size,
            copy ? FOS_MAILBOX_OPTIONS_URPC_1 :
                   FOS_MAILBOX_OPTIONS_URPC_2 | FOS_MAILBOX_OPTIONS_NO_COPY /* options */);    

    dispatchRegisterHandlers(
        m_public_mbox->ref(),
        sizeof(handlers) / sizeof(handlers[0]),
        handler_types,
        handlers,
        NULL);

    // idle loop seems unnecessary for the time being
//    dispatchRegisterIdleHandler(::idle, NULL);
}

void Link::init_devices()
{
    m_phys.push_back(new fos::net::phy::Eth(*this));
}

Link::~Link()
{
    printf("Shutting down link layer.\n");

    for (size_t i = 0; i < m_phys.size(); i++)
    {
        delete m_phys[i];
    }
    m_phys.resize(0);

    check(
        m_public_mbox->unreg(Alias("/sys/net/@local/link")));
    delete m_public_mbox;

    // Need ref counting to safely to do this. Maybe its better just to leak it. -nzb
    //     check(Alias("/sys/net/@local").release);
}

#if defined(DEBUG_NETSTACK)
static inline unsigned long readtscll(void)
{
    unsigned int a,d;
    asm volatile("rdtsc" : "=a" (a), "=d" (d));
    unsigned long ret = d;
    ret <<= 32;
    ret |= a;
    return ret;
}
#endif

void Link::quit()
{
    m_phys[0]->quit();

#ifdef CONFIG_PROF_NETSTACK
    uint64_t lo_ms_spent = ts_to_ms(g_link_lo_cycles_spent);

    fprintf(stderr, " Link (lo): %ld writes, %ld bytes, %ld cycles, %ld ms (%ld kb/s)\n", 
            g_link_lo_sends, 
            g_link_lo_bytes_sent,
            g_link_lo_cycles_spent,
            lo_ms_spent,
            lo_ms_spent ? g_link_lo_bytes_sent / lo_ms_spent : 0);
#endif


#if defined(DEBUG_NETSTACK)
      
#define array_size 1000
  #define BUFFSIZE array_size * sizeof(timing_array)
#define Packet_Test_Events 4096

    unsigned int i, j=0;
    unsigned int lastentry = 0, ulastentry = 0;push_back
    double tscval = 0.0;
    double tsccal = 100.0;

    timing_array buff[array_size];

    timing_array ubuff[array_size];


    //Calibreate timing
    tscval = readtscll();
    usleep(10000);	//wait 10 msec
    tsccal = (((double)readtscll()) - tscval)/(double)10000.0;

    //get the array of timestamps from UK space
    __do_fos_syscall_typed(SYSCALL_GET_TEST_ARRAY, (void *)&buff, BUFFSIZE, (void *)&lastentry, 0, 0, 0);

    do_dump_uta((void *)&ubuff, BUFFSIZE, &ulastentry);

    printf("Start timing printout\n");

    printf("tsccal %f ,\n", tsccal);
    //printf("taindex j= %d u=%d uk=%d ulast = %u\n", j, ubuff[j].ta_index,  buff[0].ta_index, ulastentry);

    for (i=0; i < (lastentry/ sizeof(timing_array)) ; i++)
    {
        if ( (i == 0) || (buff[i].ta_index !=  buff[i-1].ta_index) )
        {    //print next packet info from starting fifo (user space)
            printf("%d, %ld,  %d, %d, %x, %d\n", ubuff[j].event_type, ubuff[j].timestamp, ubuff[j].ta_index,
                    ubuff[j].u.info.size, ubuff[j].u.info.type, ubuff[j].u.info.protocol);
            if (ubuff[j++].ta_index != buff[i].ta_index)
                printf("Userspace fifo and UK out of sync j=%d i=%d ta_j=%d ta_i = %d jtype = %d\n", j-1, i, ubuff[j-1].ta_index, buff[i].ta_index,  ubuff[j-1].event_type );
        }

        printf("%d, %ld, %d, %12s\n", buff[i].event_type, buff[i].timestamp, buff[i].ta_index, buff[i].u.tag);
    }

    //printout userspace fifo

    printf("End timing printout\n");
#endif

}

void Link::go()
{
    dispatchRunLoop();
}

void Link::shutdown()
{
    m_link->quit();
}

/* real work */
void Link::check_back_pressure()
{
    // The purpose of this function is to limit the number of 
    if (threadGetNumThreads() > LINK_THREAD_THRESHHOLD)
    {
        if (!m_paused)
        {
            PS("pausing!\n");
            m_paused = true;

            dispatchDisableMailbox(m_public_mbox->ref());
            for (Phy::Vector::iterator p = m_phys.begin(); p != m_phys.end(); p++)
                (*p)->pause();
        }
    }
    else
    {
        if (m_paused)
        {
            m_paused = false;
            dispatchEnableMailbox(m_public_mbox->ref());
            for (Phy::Vector::iterator p = m_phys.begin(); p != m_phys.end(); p++)
                (*p)->unpause();
        }
    }
}

void Link::send(void * msg, size_t size)
{
    check_back_pressure();

    assert(m_phys.size() == 1);
    m_phys[0]->send(msg, size);

}

void Link::setMaster(const FosRemotebox & master)
{
    check_back_pressure();

    m_master_netstack = master;
    m_has_master = true;
}

MacAddress Link::getMacAddress()
{
    check_back_pressure();

    return ((Eth *)m_phys[0])->getMacAddress();
}

void Link::incoming(void * vpdata, size_t len)
{
    if(!m_has_master) return;
    check_back_pressure();
    link_handle_incoming(vpdata, len, &m_master_netstack.ref());
}

/* Entry point */
int main(int argc, char ** argv)
{
    Link link;
    link.go();
    return 0;
}
