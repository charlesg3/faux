#include "./include/hash.h"

unsigned int RSNetHash(in_addr_t local_ip, int local_port, in_addr_t remote_ip, int remote_port);

unsigned int JSNetHash(in_addr_t local_ip, int local_port, in_addr_t remote_ip, int remote_port)
{
    return RSNetHash(local_ip, local_port, remote_ip, remote_port);
    unsigned int hash = 1315423911;
    unsigned int i    = 0;

    char *str = (char *)&local_ip;
    for(i = 0; i < sizeof(local_ip); str++, i++)
        hash ^= ((hash << 5) + (*str) + (hash >> 2));

    str = (char *)&local_port;
    for(i = 0; i < sizeof(local_port); str++, i++)
        hash ^= ((hash << 5) + (*str) + (hash >> 2));

    str = (char *)&remote_ip;
    for(i = 0; i < sizeof(remote_ip); str++, i++)
        hash ^= ((hash << 5) + (*str) + (hash >> 2));

    str = (char *)&remote_port;
    for(i = 0; i < sizeof(remote_port); str++, i++)
        hash ^= ((hash << 5) + (*str) + (hash >> 2));

    return hash;
}

unsigned int RSNetHash(in_addr_t local_ip, int local_port, in_addr_t remote_ip, int remote_port)
//unsigned int RSHash(char* str, unsigned int len)
{
    unsigned int b    = 378551;
    unsigned int a    = 63689;
    unsigned int hash = 0;
    unsigned int i    = 0;

    char *str = (char *)&local_ip;
    for(i = 0; i < sizeof(local_ip); str++, i++)
    {
        hash = hash * a + (*str);
        a    = a * b;
    }

    str = (char *)&local_port;
    for(i = 0; i < sizeof(local_port); str++, i++)
    {
        hash = hash * a + (*str);
        a    = a * b;
    }

    str = (char *)&remote_ip;
    for(i = 0; i < sizeof(remote_ip); str++, i++)
    {
        hash = hash * a + (*str);
        a    = a * b;
    }

    str = (char *)&remote_port;
    for(i = 0; i < sizeof(remote_port); str++, i++)
    {
        hash = hash * a + (*str);
        a    = a * b;
    }

    return hash;
}
