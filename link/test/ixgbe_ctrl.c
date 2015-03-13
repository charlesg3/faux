#include <stdio.h>
#include <sys/ioctl.h>
#include <net/netif.h>
#include <sys/time.h>

#define NUM_PACKETS 100000

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("You must specify an interface\n");
        return 1;
    }

    netif_t nif;
    int r = netif_init(&nif, argv[1], 0);
    if(r < 0)
    {
        printf("Open failed\nReturned %d\n", -r);
        return 1;
    }

    r = netif_bind_cpu(&nif, 1, 0);
    if(r < 0)
    {
        printf("cpu bind failed\n");
        return -r;
    }

    struct netmap_if_opts opts;
    r = ioctl(nif.fd, NIOCGETOPTS, &opts);
    if(r != 0)
    {
        printf("get opts failed %d\n", r);
        perror("err");
        return r;
    }

    printf("Got opts %lx from kernel\n", opts.nopts_flags);

    opts.nopts_flags = 1337;
    r = ioctl(nif.fd, NIOCSETOPTS, &opts); 
    if(r != 0)
    {
        printf("set opts failed %d\n", r);
        perror("err");
        return r;
    }


    // sleep(3);


    
    netif_close(&nif);


    return 0;
}
