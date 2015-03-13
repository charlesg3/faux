#include <cassert>
#include <iostream>

#include <name/name.h>
#include <messaging/channel.h>

#include "channel_private.h"

int
main()
{
    Endpoint *ep;
    Channel *chan;

    ep = endpoint_create();
    std::cout << "server: endpoint opened" << std::endl;

    name_init();
    name_register(SERVER_NAME, endpoint_get_address(ep));

    while ((chan = endpoint_accept(ep)) == NULL)
        ;
    std::cout << "server: channel established" << std::endl;

    for (int i = 0; i < MSG_NUM; ++i) {
        int *msg;
        size_t size;

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

        while ((msg = (int *) channel_allocate(chan, sizeof(int))) == NULL)
            ;
        *msg = i;
        channel_send(chan, msg, sizeof(int));
    }

    channel_close(chan);
    std::cout << "server: channel closed" << std::endl;
    endpoint_destroy(ep);
    std::cout << "server: endpoint closed" << std::endl;

    return 0;
}

