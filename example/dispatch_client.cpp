
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <assert.h>
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

int main()
{
    name_init();

    Address name;
    name_lookup(SERVER_NAME, &name);

    // establish a connection to the server
    Channel *server_chan;
    server_chan = dispatchOpen(name);

    // rpc_echo();
    EchoRequest *req;
    DispatchTransaction trans;
    EchoResponse *res;
    __attribute__((unused)) size_t size;

    req = (EchoRequest *) dispatchAllocate(server_chan, sizeof(EchoRequest));

    fprintf(stderr, "requesting.\n");
    trans = dispatchRequest(server_chan, SERVER_ECHO, req, sizeof(EchoRequest));

    fprintf(stderr, "waiting for response.\n");
    size = dispatchReceive(trans, (void **) &res);
    assert(size == sizeof(EchoResponse));
    dispatchFree(trans, res);

    fprintf(stderr, "done.\n");
    return 0;
}
