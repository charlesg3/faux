#pragma once

typedef struct
{
    int id;
    Address ep_addr;
} LinkRegisterTransportRequest;

typedef struct
{
    bool ack;
} LinkRegisterTransportResponse;

typedef struct
{
    char MAC[6];
} LinkGetMACResponse;

typedef struct 
{
    int id;
} LinkUnregisterTransportRequest;

typedef struct
{
    bool ack;
} LinkUnregisterTransportResponse;

typedef struct
{
    bool _unused;
} LinkQuitNotification;

