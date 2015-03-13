#include <assert.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <messaging/channel.h>

#include <common/util.h>

#define NUM_MSG (1000000)

int
main(void)
{
    Address *server_ep_addr;
    pid_t pid;

    server_ep_addr = (Address *) mmap(NULL, sizeof(Address), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    assert(server_ep_addr != MAP_FAILED);
    *server_ep_addr = (Address) ~0;

    pid = fork();
    assert(pid != -1);
    if (pid) {
        Endpoint *ep;
        Channel *chan;

        ep = endpoint_create();
        *server_ep_addr = endpoint_get_address(ep);
        wmb();
        while ((chan = endpoint_accept(ep)) == NULL)
            ;
        channel_close(chan);
        endpoint_destroy(ep);
    } else {
        Endpoint *ep;
        Channel *chan;

        while (*server_ep_addr == (Address) ~0)
            cpu_relax();

        ep = endpoint_create();
        chan = endpoint_connect(ep, *server_ep_addr);
        channel_close(chan);
        endpoint_destroy(ep);
    }

    munmap(server_ep_addr, sizeof(Address));
    return 0;
}

