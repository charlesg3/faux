#include <stdio.h>
#include <sys/ioctl.h>
#include <net/netif.h>
#include <sys/time.h>

static int hex2num(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}


/**
 * hwaddr_aton - Convert ASCII string to MAC address (colon-delimited format)
 * @txt: MAC address as a string (e.g., "00:11:22:33:44:55")
 * @addr: Buffer for the MAC address (ETH_ALEN = 6 bytes)
 * Returns: 0 on success, -1 on failure (e.g., string not a MAC address)
 */
int hwaddr_aton(const char *txt, unsigned char *addr)
{
    int i;
    
    for (i = 0; i < 6; i++) {
        int a, b;
        
        a = hex2num(*txt++);
        if (a < 0)
            return -1;
        b = hex2num(*txt++);
        if (b < 0)
            return -1;
        *addr++ = (a << 4) | b;
        if (i < 5 && *txt++ != ':')
            return -1;
    }
    
    return 0;
}

void    printmac(unsigned char *mac) 
{
	printf(" '%02x:%02x:%02x:%02x:%02x:%02x' ", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}       

int main(int argc, char **argv)
{

    unsigned char MacAddress[6];


    if (argc < 2)
    {
        printf("Usage: <Interface> [MAC Address]\n");
        return 1;
    }

    netif_t nif;
    int r = netif_init(&nif, argv[1], 0);
    if(r < 0)
    {
        printf("Init failed\nReturned %d\n", -r);
        return 1;
    }
    
    // Use a default mac address
    if(argc > 2)
        hwaddr_aton(argv[2], &MacAddress[0]);
    else
        hwaddr_aton("00:01:11:12:13:14", &MacAddress[0]);
    
/*  r = ioctl(nif.fd, NIOCSETMAC, &MacAddress[0]);
    if(r != 0)
    {
        printf("set MAC failed %d\n", r);
        perror("err");
        return r;
    }
*/
    r = ioctl(nif.fd, NIOCGETMAC, &MacAddress); 
    if(r != 0)
    {
        printf("\nGet MacAddress failed %d\n", r);
        perror("err");
        return r;
    }
    
    printf("\nGot MAC ");
    printmac(&MacAddress[0]);
    printf(" from NetMap\n");
    netif_close(&nif);


    return 0;
}
