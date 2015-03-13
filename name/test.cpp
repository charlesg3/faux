#include <cassert>
#include <iomanip>
#include <iostream>
#include <messaging/channel.h>
#include <name/name.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <core/core.h>

int
main()
{
    pid_t pid = fork();
    assert(pid != -1);
    if (pid) {
        sleep(5);

        bind_to_core(1);

        name_init();
        bool found;
        Address ep_addr;
        found = name_lookup(0xdeadbeef, &ep_addr);
        assert(found);
        std::cout << "client: " << std::setw(10) << ep_addr << std::endl;
    } else {
        bind_to_core(0);

        Endpoint *ep = endpoint_create();
        std::cout << "server: " << std::setw(10) << endpoint_get_address(ep) << std::endl;
        name_init();
        name_register(0xdeadbeef, endpoint_get_address(ep));
        return 0;
    }

    waitpid(pid, NULL, 0);

    return 0;
}
