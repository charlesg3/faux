#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <atomic>
#include <thread>

std::atomic<int> go;
std::atomic<uint64_t> reqdone;
enum { sleepsec = 5 };

const char* host;
const char* port;

#define RECONNECT 0

int connect_to_server(addrinfo *ai)
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(fd >= 0);
  if(connect(fd, ai->ai_addr, ai->ai_addrlen) == -1)
      perror("connect");
  struct linger so_linger;
  so_linger.l_onoff = true;
  so_linger.l_linger = 0;
  setsockopt(fd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof so_linger);
  return fd;
}

static void
worker(int fd_)
{
  __attribute__((unused)) int r;
  int fd;
  const char* msg = "abcd";
  int msglen = strlen(msg);

  addrinfo* ai;
  r = getaddrinfo(host, port, 0, &ai);
  assert(r == 0);

#if RECONNECT
#else
  fd = connect_to_server(ai);
#endif

  while (go == 0)
    ;

  for (uint64_t i = 0; go; i++) {
#if RECONNECT
    fd = connect_to_server(ai);
#endif
    char buf[1024];
    r = write(fd, msg, msglen);
    assert(msglen == r);
    r = read(fd, buf, sizeof(buf));
    assert(msglen == r);
    r = memcmp(buf, msg, msglen);
    assert(0 == r);

    if (i > 1000) {
      reqdone += i;
      i = 0;
    }
#if RECONNECT
    close(fd);
  }
#else
  }
  close(fd);
#endif
}

int
main(int ac, char **av)
{
  if (ac != 4) {
    fprintf(stderr, "Usage: %s host port nclient\n", av[0]);
    exit(-1);
  }

  fprintf(stderr, "echo_client: started\n");

  host = av[1];
  port = av[2];
  int nthread = atoi(av[3]);

  __attribute__((unused)) int r;
  addrinfo* ai;
  r = getaddrinfo(host, port, 0, &ai);
  assert(r == 0);

  for (int i = 0; i < nthread; i++) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    assert(connect(fd, ai->ai_addr, ai->ai_addrlen) >= 0);
    std::thread(worker, fd).detach();
  }

  freeaddrinfo(ai);

  go = 1;
  sleep(sleepsec);
  printf("%lu req/sec\n", reqdone / sleepsec);
}
