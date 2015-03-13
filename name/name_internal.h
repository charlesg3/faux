#ifndef _NAME_INTERNAL_H
#define _NAME_INTERNAL_H

#include <messaging/channel.h>

#define NAME_CLOSE      ((int) 0)
#define NAME_REGISTER   ((int) 1)
#define NAME_UNREGISTER ((int) 2)
#define NAME_LOOKUP     ((int) 3)

typedef struct {
    int core;
    Name name;
    Address ep_addr;
} __attribute__ ((packed)) NameRegisterRequest;

typedef struct {
    bool ack;
} __attribute__ ((packed)) NameRegisterResponse;

typedef struct {
    int core;
    Name name;
    Address ep_addr;
} __attribute__ ((packed)) NameUnregisterRequest;

typedef struct {
    bool ack;
} __attribute__ ((packed)) NameUnregisterResponse;

typedef struct {
    int core;
    Name name;
} __attribute__ ((packed)) NameLookupRequest;

typedef struct {
    Address ep_addr;
    bool ack;
} __attribute__ ((packed)) NameLookupResponse;

typedef struct {
    int __unused;
} __attribute__ ((packed)) NameCloseNotification;

#endif // _NAME_INTERNAL_H

