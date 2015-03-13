#include "param.h"
#include "compiler.h"
#include "distribution.hh"
#include "spinbarrier.hh"
#include "libutil.h"
#include "xsys.h"
#include <utilities/tsc.h>

#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <string>
#include <thread>

// Set to 1 to manage the queue manager's life time from this program.
// Set to 0 if the queue manager is started and stopped outside of
// mailbench.
#define START_QMAN 1

#define MAIL_MSGS 8192

extern int
enqueue_main(int argc, const char **argv);

using std::string;

enum { warmup_usecs = 50000 };
enum { duration = 5 };

static struct sharedmem {
    volatile bool stop __mpalign__;
    volatile bool warmup;
    spin_barrier bar;
    concurrent_distribution<uint64_t> start_tsc, stop_tsc;
    concurrent_distribution<uint64_t> start_usec, stop_usec;
    concurrent_distribution<uint64_t> count;
} *shared;

const char *message =
  "Received: from incoming.csail.mit.edu (incoming.csail.mit.edu [128.30.2.16])\n"
  "        by metroplex (Cyrus v2.2.13-Debian-2.2.13-14+lenny5) with LMTPA;\n"
  "        Tue, 19 Mar 2013 22:45:50 -0400\n"
  "X-Sieve: CMU Sieve 2.2\n"
  "Received: from mailhub-auth-3.mit.edu ([18.9.21.43])\n"
  "        by incoming.csail.mit.edu with esmtps\n"
  "        (TLS1.0:DHE_RSA_AES_256_CBC_SHA1:32)\n"
  "        (Exim 4.72)\n"
  "        (envelope-from <xxxxxxxx@MIT.EDU>)\n"
  "        id 1UI92E-0007D2-7N\n"
  "        for xxxxxxxxx@csail.mit.edu; Tue, 19 Mar 2013 22:45:50 -0400\n"
  "Received: from outgoing.mit.edu (OUTGOING-AUTH-1.MIT.EDU [18.9.28.11])\n"
  "        by mailhub-auth-3.mit.edu (8.13.8/8.9.2) with ESMTP id r2K2jnO5025684\n"
  "        for <xxxxxxxx@mit.edu>; Tue, 19 Mar 2013 22:45:49 -0400\n"
  "Received: from xxxxxxxxx.csail.mit.edu (xxxxxxxxx.csail.mit.edu [18.26.4.91])\n"
  "        (authenticated bits=0)\n"
  "        (User authenticated as xxxxxxxx@ATHENA.MIT.EDU)\n"
  "        by outgoing.mit.edu (8.13.8/8.12.4) with ESMTP id r2K2jmc7032022\n"
  "        (version=TLSv1/SSLv3 cipher=DHE-RSA-AES128-SHA bits=128 verify=NOT)\n"
  "        for <xxxxxxxx@mit.edu>; Tue, 19 Mar 2013 22:45:49 -0400\n"
  "Received: from xxxxxxx by xxxxxxxxx.csail.mit.edu with local (Exim 4.80)\n"
  "        (envelope-from <xxxxxxxx@mit.edu>)\n"
  "        id 1UI92C-0000il-4L\n"
  "        for xxxxxxxx@mit.edu; Tue, 19 Mar 2013 22:45:48 -0400\n"
  "From: Austin Clements <xxxxxxxx@MIT.EDU>\n"
  "To: xxxxxxxx@mit.edu\n"
  "Subject: Test message\n"
  "User-Agent: Notmuch/0.15+6~g7d4cb73 (http://notmuchmail.org) Emacs/23.4.1\n"
  "        (i486-pc-linux-gnu)\n"
  "Date: Tue, 19 Mar 2013 22:45:48 -0400\n"
  "Message-ID: <874ng6vler.fsf@xxxxxxxxx.csail.mit.edu>\n"
  "MIME-Version: 1.0\n"
  "Content-Type: text/plain; charset=us-ascii\n"
  "\n"
  "Hello.\n";

extern char **environ;



static __padout__ __attribute__((unused));

static void 
initshared(void)
{
    if(shared)
        free(shared);

    shared = (struct sharedmem *) mmap(0, sizeof(struct sharedmem), PROT_READ|PROT_WRITE, 
                                       MAP_SHARED|MAP_ANONYMOUS, 0, 0);

    shared->warmup = false;
    shared->stop = false;
    if (shared == MAP_FAILED) {
      perror("mmap failed");
      exit(-1);
    }
}

static void
timer_thread(void)
{
  shared->warmup = true;
  shared->bar.join();
  usleep(warmup_usecs);
  shared->warmup = false;
  if(! MAIL_MSGS)
  {
      sleep(duration);
      shared->stop = true;
  }
}

static void
xwaitpid(int pid, const char *cmd)
{
  int status;
  if (waitpid(pid, &status, 0) < 0)
    edie("waitpid %s failed", cmd);
  if (!WIFEXITED(status) || WEXITSTATUS(status))
    die("status %d from %s", status, cmd);
}

static void
do_mua(int cpu, string spooldir, string msgpath)
{
  char *argv[] = {"mail-enqueue", (char *)spooldir.c_str(), "user", nullptr};
#if defined(XV6_USER)
  int errno;
#endif
  //setaffinity(cpu);
  shared->bar.join();

  // Open message file (alternatively, we could use an open spawn
  // action)
  int msgfd = open(msgpath.c_str(), O_RDONLY|O_CLOEXEC|O_ANYFD);
  if (msgfd < 0)
    edie("open %s failed", msgpath.c_str());

  dup2(msgfd, 0);

  bool mywarmup = true;
  uint64_t mycount = 0;
  while (!shared->stop) {
    if (__builtin_expect(shared->warmup != mywarmup, 0)) {
      mywarmup = shared->warmup;
      mycount = 0;
      shared->start_usec.add(now_usec());
      shared->start_tsc.add(rdtscll());
    }

    if (lseek64(msgfd, 0, SEEK_SET) < 0)
      edie("lseek failed");

/*
    int child_pid = fork();
    if(child_pid == 0)
    {
        execvp("mail-enqueue", argv);
    } else {
        int status;
        waitpid(child_pid, &status, 0);
    }
*/
    enqueue_main(3, (const char **)argv);
    ++mycount;

    if(MAIL_MSGS && mycount > MAIL_MSGS)
    {
        break;
    }
  }

  shared->stop_usec.add(now_usec());
  shared->stop_tsc.add(rdtscll());
  shared->count.add(mycount);
}

static void
xmkdir(const string &d)
{
  if (mkdir(d.c_str(), 01777) < 0)
    edie("failed to mkdir %s", d.c_str());
}

static void
create_spool(const string &base)
{
  xmkdir(base);
  xmkdir(base + "/pid");
  xmkdir(base + "/todo");
  xmkdir(base + "/mess");
}

static void
create_maildir(const string &base)
{
  xmkdir(base);
  xmkdir(base + "/tmp");
  xmkdir(base + "/new");
  xmkdir(base + "/cur");
}

void
usage(const char *argv0)
{
  fprintf(stderr, "Usage: %s [options] basedir nthreads\n", argv0);
  fprintf(stderr, "  -a none   Use regular APIs (default)\n");
  fprintf(stderr, "     all    Use alternate APIs\n");
  exit(2);
}

static void bind_to_core(int core)
{
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(core, &cpu_set);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set);

    while(sched_getcpu() != core)
        usleep(0);
}

int
main(int argc, char **argv)
{
  const char *alt_str = "false";
  int opt;
  while ((opt = getopt(argc, argv, "a:")) != -1) {
    switch (opt) {
    case 'a':
      alt_str = optarg;
      break;
    default:
      usage(argv[0]);
    }
  }

  if (argc - optind != 2)
    usage(argv[0]);

  bind_to_core(0);

  string basedir(argv[optind]);
  const char *nthreads_str = argv[optind+1];
  int nthreads = atoi(nthreads_str);
  if (nthreads <= 0)
    usage(argv[0]);

  // Create spool and inboxes
  // XXX This terminology is wrong.  The spool is where mail
  // ultimately gets delivered to.
  string spooldir = basedir + "/spool";
  if (START_QMAN)
    create_spool(spooldir);
  string mailroot = basedir + "/mail";
  xmkdir(mailroot);
  create_maildir(mailroot + "/user");

  initshared();

  pid_t qman_pid;
  if (START_QMAN) {
    // Start queue manager
    char *qman[] = {"mail-qman", "-a", (char *)alt_str,
                          (char *)spooldir.c_str(), (char *)mailroot.c_str(),
                          (char *)nthreads_str, (char *)nullptr};
    qman_pid = fork();
    if(qman_pid < 0)
      die("posix_spawn %s failed", qman[0]);
    if(qman_pid == 0)
    {
        char **qman_l = (char **)malloc(sizeof(char *) * 7);
        memcpy((void *)qman_l, qman, sizeof(qman));
        execvp("mail-qman", qman_l);
    }
    sleep(1);
  }

  // Write message to a file
  int fd = open((basedir + "/msg").c_str(), O_CREAT|O_WRONLY, 0666);
  if (fd < 0)
    edie("open");
  xwrite(fd, message, strlen(message));
  close(fd);

  printf("# --cores=%d --duration=%ds --alt=%s\n", nthreads, duration, alt_str);

  // Run benchmark
  shared->bar.init((nthreads / 2) + 1);

  pid_t timer = fork();
  if(timer < 0)
      die("fork fail()");
  if(timer == 0)
  {
      timer_thread();
      exit(0);
  }


  pid_t children[nthreads / 2];
  for (int i = 0; i < nthreads / 2; ++i)
  {
      children[i] = fork();
      if(children[i] < 0)
          die("fork fail()");
      else if(children[i] == 0)
      {
          bind_to_core(i);
          do_mua(i, basedir + "/spool", basedir + "/msg");
          exit(0);
      }
  }

  // Wait
  xwaitpid(timer, "timer_thread");

  for (int i = 0; i < nthreads / 2; ++i)
    xwaitpid(children[i], "mua child");

  if (START_QMAN) {
    // Kill qman and wait for it to exit
    char *enq[] = {"mail-enqueue", "--exit", (char *)spooldir.c_str(), nullptr};
    enqueue_main(3, (const char **)enq);
    xwaitpid(qman_pid, "mail-qman");
  }

  // Summarize
  printf("%llu start usec skew\n", shared->start_usec.span());
  printf("%llu stop usec skew\n", shared->stop_usec.span());
  uint64_t usec = shared->stop_usec.mean() - shared->start_usec.mean();
  printf("usec: %llu\n", usec);
  printf("%.2f secs\n", (double)usec / (double)1e6);
  printf("%llu cycles\n", shared->stop_tsc.mean() - shared->start_tsc.mean());

  uint64_t messages = shared->count.sum();
  printf("%llu messages\n", messages);
  if (messages) {
    printf("%llu cycles/message\n",
           (shared->stop_tsc.sum() - shared->start_tsc.sum()) / messages);
    printf("%llu messages/sec\n", messages * 1000000 / usec);
  }

  printf("\n");
}

