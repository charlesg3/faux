#include <messaging/channel.h>

int
main(void)
{
    Endpoint *ep;

    ep = endpoint_create();
    endpoint_destroy(ep);
    return 0;
}

