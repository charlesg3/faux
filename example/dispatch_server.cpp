#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <stdio.h>

#include <messaging/dispatch.h>
#include <messaging/channel.h>
#include <name/name.h>

#define SERVER_ECHO 1
#define SERVER_NAME 42

typedef struct
{
    int blah;
} EchoRequest;

typedef struct
{
    int blah;
} EchoResponse;

void echo(Channel* chan, DispatchTransaction trans, void* msg, size_t size);

void idle(void *);

int main()
{

    Endpoint *endp = endpoint_create();
    name_init();
    name_register(SERVER_NAME, endpoint_get_address(endp));

    dispatchListen(endp);
    dispatchRegister(SERVER_ECHO, echo);

    dispatchRegisterIdle(idle, NULL);

    dispatchStart();
    return 0;
}

void echo(Channel* chan, DispatchTransaction trans, void* msg, size_t size)
{
    fprintf(stderr, "echoing.\n");
    assert(msg != NULL);
    assert(size == sizeof(EchoRequest));

    EchoRequest* req = (EchoRequest*) msg;
    int blah = req->blah;
    dispatchFree(trans, msg);

    EchoResponse* res = (EchoResponse*) dispatchAllocate(chan, sizeof(EchoResponse));
    res->blah = blah;
    dispatchRespond(chan, trans, res, sizeof(EchoResponse));
}


void idle(void * ignored)
{
}
