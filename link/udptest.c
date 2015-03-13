/*
 * UDP TEST PROGRAM
 * Chris Johnson <crjohns@csail.mit.edu>
 *
 *
 * Usage: udptest <ip> [num threads] [local port] [port skip]
 *
 * Run UDP echo test against <ip>.
 * This will send packets to <ip>:1024 and wait for replies.
 * By default, this does 100K echoes and outputs information about
 * the round trip times for those packets when done.
 *
 *
 * Optional arguments:
 * 
 * num threads is the number of threads to spawn. Each one of these
 * does 100K echoes. This defaults to 1.
 *
 * local port is the source port to bind to. This defaults to 0 (random port).
 *
 * port skip is used to allocate different ports to different threads.
 * This defaults to 1, which allocates ports sequentially.
 * So running udptest <ip> 4 10000 would send from ports
 * 10000, 10001, 10002, 10003 on different threads.
 *
 * Changing this to udptest <ip> 4 10000 2 would send from
 * ports 10000, 10002, 10004, 10006 on different threads.
 *
 * port skip is used so that different machines may run
 * all threads on different ports across machines when the local
 * port is assigned sequentially (as it is in scripts):
 *
 * machine1 runs udptest <ip> 2 10000 2
 * machine2 runs udptest <ip> 2 10001 2
 *
 * machine1 sends from 10000 and 10002
 * machine2 sends from 10001 and 10003
 *
 */




#define _XOPEN_SOURCE 600
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>

#define NUM_TRIES 100000


pthread_barrier_t barrier;


struct thread_args
{
    struct sockaddr_in *saddr;
    int port;
};

void *runtest(void *arg)
{
    struct thread_args *args = (struct thread_args*) arg;
    struct sockaddr_in *saddr = args->saddr;
    int port = args->port;
 
    struct timeval begin;
    struct timeval end;
    gettimeofday(&begin, NULL);

    char *buf = "crj\n";
    char rbuf[4];

    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if(port)
    {
        struct sockaddr_in local;
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(port);
        local.sin_addr.s_addr = INADDR_ANY;

        int r = bind(fd, (struct sockaddr*)&local, sizeof(local));
        if(r != 0)
        {
            perror("Failed to bind");
            exit(1);
        }
    }

    pthread_barrier_wait(&barrier);

    int i;
    for(i=0; i<NUM_TRIES; i++)
    {
        sendto(fd, buf, 4, 0, (struct sockaddr*)saddr, sizeof(struct sockaddr_in));
        recvfrom(fd, rbuf, 4, 0, NULL, NULL);
        //if(memcmp(buf, rbuf, 4) != 0)
        //{
            //fprintf(stderr, "Got invalid echo\n");
        //}
    }

    gettimeofday(&end, NULL);

    double elapsed = (end.tv_sec + end.tv_usec / 1000000.0) - 
                     (begin.tv_sec + begin.tv_usec / 1000000.0);

    printf("Did %d requests in %f seconds (%f / sec)\n", NUM_TRIES, elapsed, NUM_TRIES/elapsed);

    return NULL;
}

int main(int argc, char *argv[])
{
    int num_threads = 1;
    int port = 0;
    int port_skip = 1;

    if(argc < 2)
    {
        printf("Usage: %s <ip> [num threads] [local port] [port skip]\n", argv[0]);
        printf("\tRun echo test against given ip.\n"
               "\tWhen num threads is set, spawn and synchronize that number of threads\n"
               "\tWhen local port is set, use that local port number\n"
               "\tWhen port skip is set, thread i gets port localport+(i*skip)\n");
        return 1;
    }

    if(argc >= 3)
        num_threads = atoi(argv[2]);

    if(argc >= 4)
        port = atoi(argv[3]);
    
    if(argc >= 5)
        port_skip = atoi(argv[4]);


    pthread_barrier_init(&barrier, NULL, num_threads);

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(1024);
    saddr.sin_addr.s_addr = inet_addr(argv[1]);

    if(saddr.sin_addr.s_addr == INADDR_NONE)
    {
        printf("Invalid IP\n");
        return 2;
    }


    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    char *buf = "tmp\n";
    char rbuf[4];

    // Send a few requests to get ARP stuff out of the way
    int i;
    for(i=0; i<3; i++)
    {
        sendto(fd, buf, 4, 0, (struct sockaddr*)&saddr, sizeof(saddr));
        recvfrom(fd, rbuf, 4, 0, NULL, NULL);
    }

    close(fd);

    pthread_t threads[num_threads];

    struct thread_args args[num_threads];

    for(i=0; i<num_threads; i++)
    {
        memset(&args[i], 0, sizeof(args[i]));
        if(port)
        {
            args[i].port = port;
            port += port_skip;
        }
        args[i].saddr = &saddr;

        pthread_create(&threads[i], NULL, runtest, &args[i]);
    }

    for(i=0; i<num_threads; i++)
        pthread_join(threads[i], NULL);

    return 0;
}
