#!/usr/bin/python
import os
import subprocess
import signal
import select
import re
import sys
import shlex
import signal
import errno
from time import sleep


# Usage:
# faux.py <programs>
# programs are either "programname" or "programname=<args>",
# where args are the arguments to that program.
# The programname is the name of the program in the list
# below, which gives information on how to start the program.
#
# Examples:
#
# Start webserver:
# faux.py nameserver netstack webserver
#
# Alternative interface:
# faux.py nameserver netstack=eth7 webserver
#
# More options:
# faux.py nameserver linkserver="eth5 0"



# List of programs
# Format is: (name, file, output regex, args, envs)
programs = [
      ('nameserver', 'build/name/nameserver',
                     r'nameserver: started', None, None),
      ('server', 'build/sys/server/server',
                     r'filesystem: started', None, None),
    ]

def wait_for_output(prog, pipe_list, match):
    while True:
        out,dummy1,dummy2 = select.select(pipe_list, [], [], 0.01)
        for o in out:
            output = o.readline()
            if output == '':
                if prog.poll():
                    # Print out all output before exiting
                    for o in out:
                        output = o.readline()
                        if output:
                            print output
                    print "Failed to get required output"
                    return False
            else:
                print output,
            m = re.match(match, output)
            if m:
                if "ip address" in match:
                    global ip_address
                    ip_address = m.group(1)
                return True
        poll_outputs()

def start_program(program):
    global ip_address
    global unique_port
    (name, filename, regex, args, envs) = program
    pargs = [filename]
    if args:
        pargs = shlex.split(filename + " " + args)

    penv = os.environ.copy()

    if envs:
        for env in envs.split(','):
            [arg1, arg2] = env.split('=')
            penv[arg1] = arg2
            #print "Overriding '%s' env: %s=%s" % (name, arg1, arg2)

    pargs = map(lambda x: x.replace("IP_ADDRESS", ip_address), pargs)

    # print "Starting %s ( args: %s)" % (name, pargs)
    print "Starting %s" % (name)

    # Must close fds or subprocesses may crash
    prog = subprocess.Popen(pargs, env=penv, stdout = subprocess.PIPE, stderr = subprocess.PIPE, close_fds=True)
    if regex:
        if not wait_for_output(prog, [prog.stdout, prog.stderr], regex):
            raise Exception('Failed to get required output')
    return prog

def start_faux():
    global programs
    global procs
    global outs
    print " -- starting faux --"
    try:
        for p in programs:
            subproc = start_program(p)
            procs += [subproc]
            outs += [subproc.stdout]
            outs += [subproc.stderr]
    except:
        for prog in programs:
            subprocess.call(shlex.split("killall -9 %s" % prog[1]))
        exit(1)

    i = 0
    for p in programs:
        if p[0] == "ab":
            programs[i] = (programs[i][0] + "-stopped", programs[i][1], programs[i][2], programs[i][3], programs[i][4])
            stop_faux()

        i += 1

    print " -- faux started --"

def stop_faux():
    global procs
    global programs
    print "Stopping faux..."
    i = 0
    for proc in procs:
        if not "-stopped" in programs[i][0]:
            try:
                proc.send_signal(signal.SIGINT);
                for j in range(1,1000):
                    out,dummy1,dummy2 = select.select([proc.stdout, proc.stderr], [], [], 0.001)
                    for o in out:
                        output = o.readline()
                        if output != '':
                            print output,
                    if proc.poll() != None:
                        programs[i] = (programs[i][0] + "-stopped", programs[i][1], programs[i][2], programs[i][3], programs[i][4])
                        break;

                i+=1
            except:
                print "error killing: %s" % programs[i][0]
                i+=1
                continue

    for p in programs:
        if not "-stopped" in p[0]:
            subprocess.call(shlex.split("killall -9 %s" % p[1]))

    os.system("rm -rf /dev/shm/faux-shm-*")

    exit(0)

# Parse the command line to make a program list
def parse_commandline():
    global programs

    ret = []
    for arg in sys.argv[1:]:
        argl = arg.split('=', 1)

        if len(argl) == 0:
            continue
        name = argl[0]
        args = None
        if len(argl) == 2:
            args = argl[1]

        matchprogs = filter(lambda x: x[0] == name, programs)
        if len(matchprogs) == 0:
            raise Exception("Program name '%s' not found" % name)
        (pname, fname, regex, pargs, env) = matchprogs[0]

        # Use alternate args if set
        if args:
            pargs = args

        ret += [(pname, fname, regex, args, env)]

    return ret

def poll_outputs():
    global outs
    global procs
    global programs
    out,dummy1,dummy2 = select.select(outs, [], [], 0.01)
    for o in out:
        output = o.readline()
        if output:
            print output,
    i = 0
    for p in procs:
        if p.poll():
            print "Program %s crashed (%d)\n" % (programs[i][0], p.poll())
            stop_faux()
            exit(1)
        if p.poll() == 0 and programs[i][0] != 'ab':
            if programs[i][0] == 'ab':
                programs = filter(lambda x: x[0] != 'ab', programs)
            else:
                print "Program %s exited normally (%d)\n" % (programs[i][0], p.poll())
                stop_faux()
                exit(1)
        i += 1

def sig_handle(arg1, arg2):
    stop_faux()
    exit(0)

ip_address = ""
procs = []
outs = []
programs = parse_commandline()
start_faux()

signal.signal(signal.SIGINT, sig_handle)

while True:
    poll_outputs()


