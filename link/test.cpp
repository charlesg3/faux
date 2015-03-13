#include <stdio.h>
#include <net/fauxlink.h>


int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        printf("Usage %s <interface>\n", argv[0]);
        exit(1);
    }


    int rings = LocalNetworkLink::getRingCount(argv[1]);
    LocalNetworkLink *link = new LocalNetworkLink(argv[1], 0, rings);

    // Who has 18.17.1.1? Tell 142.1.0.1
    char *hi = (char*)"\xff\xff\xff\xff\xff\xff\x00\x01\x11\x12\x13\x14\x08\x06\x00\x01\x08\x00\x06\x04\x00\x01\x01\x11\x12\x13\x14\x15\x8e\x01\x00\x01\xae\x11\xad\x12\x00\x01\x12\x11\x01\x01";
    link->send(hi, 42);

    NetworkPacketList *list = new NetworkPacketList();

    while(true)
    {
        printf("waiting for packets\n");
        link->receive(list, NULL);

        while(list->next())
        {
            char *buf;
            uint16_t len;
            list->get(&buf, &len);
            fprintf(stderr, "Got a %d byte packet\n", len);
        }
    }

}
