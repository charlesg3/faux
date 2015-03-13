#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <thread>
#include <messaging/threading.h>

int listen_fd;

#define SO_SHARED 0x4000

static void client(void *vpfd)
{
  __attribute__((unused)) int r;
  int fd = (int) (intptr_t) vpfd;
  for (;;) {
    char buf[1024];
    ssize_t cc = read(fd, buf, sizeof(buf));
    if (cc < 0 && errno == EWOULDBLOCK)
    {
        threadYield(true);
        continue;
    }
    else if(cc <= 0)
      break;

    r = write(fd, buf, cc);
    assert(cc == r);
  }
  close(fd);
}

void spawn(int n) {
  for (int i = 0; i < n-1; i++) {
      if (fork() == 0) {
          return;
      }
  }
}

void acceptLoop(void *v)
{
    for(;;)
    {
        struct sockaddr_in csin;
        socklen_t csinlen = sizeof(csin);
        int cfd = accept4(listen_fd, (sockaddr*) &csin, &csinlen, SOCK_NONBLOCK);
        if(cfd >= 0)
        {
            int val = fcntl (cfd, F_GETFL, 0); 
            fcntl (cfd, F_SETFL, val | O_NONBLOCK);
            threadSchedule(threadCreate(client, (void *)(intptr_t)  cfd));
        }
        threadYield(true);
    }
}

int
main(int ac, char **av)
{
  if (ac != 3) {
    fprintf(stderr, "Usage: %s port nservers\n", av[0]);
    exit(-1);
  }

  __attribute__((unused)) int r;
  int port = atoi(av[1]);
  int nservers = atoi(av[2]);
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  assert(listen_fd >= 0);

  setsockopt(listen_fd, SOL_SOCKET, SO_SHARED, (void *)true, sizeof(int));
  int on = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);
  r = bind(listen_fd, (sockaddr*) &sin, sizeof(sin));
  assert(r >= 0);

  spawn(nservers);

  r = listen(listen_fd, 1024);
  assert(r >= 0);

  fprintf(stderr, "echo_server: started.\n");

  threadSchedule(threadCreate(acceptLoop, NULL));
  threadingStart(NULL, NULL);
}

