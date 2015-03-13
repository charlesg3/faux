#!/usr/bin/python

from fabric.operations import *
import fabric
import sys
import getpass
import os
import select
import random
import string
import re
import pty


def usage():
    print "Usage: %s [user@]server[,...] scriptpath [output=<file>] <args>" % sys.argv[0]
    print "Run a local script on the remote server listed"
    exit(0)

def idgen():
    random.seed(os.getpid())
    return ''.join(random.choice(string.digits + string.ascii_lowercase) for x in range(7)) + "-"

if len(sys.argv) < 3:
    usage()


hosts = []
localscript = ""
remoteargs = ""
outputfile = ""
syncwait = 0

unique_port = 10000

def parse_cmdline():
    global hosts
    global localscript
    global remoteargs
    global outputfile
    global syncwait
    global env

    hosts = sys.argv[1].split(",")
    pairs = map(lambda x: x.split("@"), hosts)
    pairs = map(lambda x: x if len(x) == 2 else ["fauxtest", x[0]], pairs)
    hosts = map(lambda x: '@'.join(x), pairs)

    localscript = sys.argv[2]

    args = sys.argv[3:]
    if len(args) > 0:
        match = re.match(r"output=(.+)", args[0])
        if match:
            outputfile = match.groups(1)[0]
            args.pop(0)
    if len(args) > 0:
        match = re.match(r"sync=(\d+)", args[0])
        if match:
            syncwait = int(match.groups(1)[0])
            args.pop(0)


    remoteargs = ' '.join(args)

def format_time(t):
    return time.strftime("%H:%M:%S", time.localtime(t))

parse_cmdline()


if not localscript:
    print("No local script listed")
    usage()



synctime = None

if syncwait > 0:
    synctime = time.time() + syncwait


is_parent = False
single = True

if outputfile:
    f = open(outputfile, "w+")
    f.truncate()
    f.close()

# Each child has a unique id starting at 0
childid = 0

# Fork handler for each host
if len(hosts) > 1:
    subprocs = []
    fds = []
    single = False
    while len(hosts) != 0:
        host = hosts.pop(0)
        pid, fd = pty.fork()
        if pid == 0:
            env.host_string = host
            break
        else:
            childid += 1
            subprocs.append(pid)
            fds.append(fd)
    else:
        # Execute when we are parent
        is_parent = True

        fds = map(lambda x: os.fdopen(x), fds)

        closed = 0
        while True:
            ready, _, _ = select.select(fds, [], [])
            for fd in ready:
                try:
                    line = fd.readline()
                    if line:
                        print line,
                except IOError:
                    fds.remove(fd)

            if len(fds) == 0:
                break

        for p in subprocs:
            os.waitpid(p, 0)
else: 
    # Single connection case; no children
    env.host_string = hosts[0]



if not is_parent:
    _, filename = os.path.split(localscript)
    remotescript = "/tmp/" + idgen() + filename

    print "Uploading %s to %s" % (localscript, remotescript)
    status = put(localscript, remotescript, mode=0755)
    if status.failed:
        print "Upload failed"
        exit(1)


    remoteargs = remoteargs.replace("UNIQUE_PORT", str(unique_port + childid), 1)

    print "Running %s remotely %s" % (remotescript, "" if not synctime else "at "+ format_time(synctime))
    if synctime:
        while time.time() < synctime:
            pass

    output = run(remotescript + " " + remoteargs, pty=False)
    run("rm " + remotescript, pty=False)

    if outputfile:
        print "Writing output to", outputfile
        f = open(outputfile, 'a+')
        f.write(output+"\n")
        f.close()

if is_parent or single:
    print "runremote: done"
