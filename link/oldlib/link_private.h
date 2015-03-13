#pragma once

#include <dispatch/dispatch.h>
#include <sys/net/phy/netif.h>

typedef struct {
    size_t size;
    unsigned char data[0];
} __attribute__((__packed__)) LinkSendMessage;

typedef struct {
    FosRemotebox reply;
    size_t size;
    unsigned char data[0];
} __attribute__((__packed__)) LinkIncomingMessage;


#define LINK_SEND_MESSAGE_HDR_SIZE (sizeof(LinkSendMessage))

typedef struct {
    DispatchMessageHeader hdr;
    FosRemotebox reply;
    FosRemotebox master;
} LinkSetMasterMessage;

typedef struct {
    DispatchMessageHeader hdr;
    FosStatus status;
} LinkSetMasterResponse;

typedef struct {
    DispatchMessageHeader hdr;
    FosRemotebox reply;
} LinkGetMacMessage;

typedef struct {
    DispatchMessageHeader hdr;
    MacAddress mac;
    FosStatus status;
} LinkGetMacResponse;

typedef struct {
    DispatchMessageHeader hdr;
    FosRemotebox reply;
} LinkShutdownMessage;

typedef struct {
    DispatchMessageHeader hdr;
    FosStatus status;
} LinkShutdownResponse;

typedef struct {
    DispatchMessageHeader hdr;
    FosStatus status;
} LinkIncomingResponse;

enum LinkMessageTypes {
    NET_LINK_INCOMING,
    NET_LINK_SEND,
    NET_LINK_SET_MASTER,
    NET_LINK_GET_MAC_ADDRESS,
    NET_LINK_SHUTDOWN
};


#ifdef __cplusplus
extern "C" {
#endif 

void link_handle_incoming(void *vpdata, size_t len, FosRemotebox * master_netstack);

#ifdef __cplusplus
}
#endif 

