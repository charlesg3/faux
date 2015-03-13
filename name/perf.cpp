#include <common/tsc.h>
#include <common/util.h>
#include <iostream>
#include <messaging/channel.h>
#include <name/name.h>
#include <vector>

#define NUM_OPS (200)

int
main()
{
    uint64_t begin, end;
    Endpoint *ep = endpoint_create();
    Address ep_addr = endpoint_get_address(ep);

    name_init();

    begin = rdtsc64();
    for (Name name = 1; name <= NUM_OPS; ++name)
        name_register(name, ep_addr);
    end = rdtsc64();
    std::cout << static_cast<double>(end - begin) / NUM_OPS << " cycles/register" << std::endl;

    begin = rdtsc64();
    for (Name name = 1; name <= NUM_OPS; ++name)
        name_lookup(name, &ep_addr);
    end = rdtsc64();
    std::cout << static_cast<double>(end - begin) / NUM_OPS << " cycles/lookup" << std::endl;

    begin = rdtsc64();
    for (Name name = 1; name <= NUM_OPS; ++name)
        name_unregister(name, ep_addr);
    end = rdtsc64();
    std::cout << static_cast<double>(end - begin) / NUM_OPS << " cycles/unregister" << std::endl;

    name_cleanup();

    endpoint_destroy(ep);

    return 0;
}

