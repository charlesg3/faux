#include <cassert>
#include <iostream>

#include <messaging/dispatch.h>
#include <messaging/channel.h>
#include <name/name.h>

#include "channel_private.h"

int
main()
{
    Address server_ep_addr;
    Channel *chan;
    Endpoint *ep;

    name_init();
    name_lookup(SERVER_NAME, &server_ep_addr);
    std::cout << "client: name looked up" << std::endl;

    ep = endpoint_create();
    std::cout << "client: endpoint opened" << std::endl;

    chan = endpoint_connect(ep, server_ep_addr);
    std::cout << "client: channel established" << std::endl;

    for (int i = 0; i < MSG_NUM; ++i) {
        int *msg;
        size_t size;

        while ((msg = (int *) channel_allocate(chan, sizeof(int))) == NULL)
            ;
        *msg = i;
        channel_send(chan, msg, sizeof(int));

#ifdef CONFIG_POLL_RECV
        do {
            size = channel_poll(chan);
        } while (size == 0);
        size = channel_receive(chan, (void **) &msg);
#else
        channel_receive_blocking(chan, (void **) &msg, &size);
#endif
        if (size == (size_t) -1)
            break;
        assert(size == sizeof(int));
        assert(*msg == i);
        channel_free(chan, msg);
    }

    channel_close(chan);
    std::cout << "client: channel closed" << std::endl;
    endpoint_destroy(ep);
    std::cout << "client: endpoint closed" << std::endl;

    return 0;
}

