#ifndef _FAUXCORE_INTERNAL_H
#define _FAUXCORE_INTERNAL_H

#include <messaging/channel.h>

#define CORE_GET        ((int) 1)

typedef struct {
    uint64_t group;
    pid_t pid;
} CoreGetRequest;

typedef struct {
    uint64_t core;
    bool ack;
} CoreGetResponse;

#endif

