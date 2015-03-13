#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h> //inet_ntop
#include <assert.h>

#include <atomic>
#include <thread>

std::atomic<int> go;
std::atomic<uint64_t> reqdone;
enum { sleepsec = 50 };

#define RECONNECT 1

void wget(int sock, char *file, char *output_filename);

static void worker(addrinfo *ai)
{
#if RECONNECT
#else
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);
    if(connect(fd, ai->ai_addr, ai->ai_addrlen) < 0)
        fprintf(stderr, "error connecting.\n");
#endif

    while (go == 0)
        ;

    for (uint64_t i = 0; go; i++)
    {
#if RECONNECT
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        assert(fd >= 0);
        fprintf(stderr, "connecting.\n");
        if(connect(fd, ai->ai_addr, ai->ai_addrlen) < 0)
            fprintf(stderr, "error connecting.\n");
        fprintf(stderr, "connected.\n");
#endif
        fprintf(stderr, "wgetting.\n");
        wget(fd, "/mem4", NULL);
        fprintf(stderr, "wgot.\n");

#if RECONNECT
        if(close(fd) != 0)
            fprintf(stderr, "error closing.\n");
#endif
        if (i > 1000) {
            reqdone += i;
            i = 0;
        }
    }

#if RECONNECT
#else
    assert(close(fd) == 0);
#endif
}


int main(int argc, char **argv)
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s host port nclient\n", argv[0]);
        exit(-1);
    }
    else
        fprintf(stderr, "wgetter: started\n");

    const char* host = argv[1];
    const char* port = argv[2];
    int nthread = atoi(argv[3]);

    addrinfo* ai;
    if(getaddrinfo(host, port, 0, &ai) != 0)
        fprintf(stderr, "error getaddrinfo(%s, %s)\n", host, port);

    for (int i = 0; i < nthread; i++)
        std::thread(worker, ai).detach();

    go = 1;
    sleep(sleepsec);
    printf("%d req/sec\n", reqdone / sleepsec);
    go = 0;
    usleep(50000);
    freeaddrinfo(ai);
    return 0;
}

