#pragma once

struct ethernet_frame {
    //    unsigned char pad[8];
    unsigned char dest[6];
    unsigned char source[6];
    uint16_t type;
} __attribute__((__packed__));

struct ip_frame {
    uint8_t version;
    uint8_t tos;
    uint16_t length;
    uint16_t id;
    uint16_t flags;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t source;
    uint32_t dest;
} __attribute__((__packed__));

struct tcp_frame {
    uint16_t source_port;
    uint16_t dest_port;
    uint32_t seq_no;
    uint32_t ack_no;
    uint16_t control;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((__packed__));

struct udp_frame {
    uint16_t source_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((__packed__));

struct arp_frame {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint8_t spa[4];
    uint8_t tha[6];
    uint8_t tpa[4];
} __attribute__((__packed__));
