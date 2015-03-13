#!/usr/bin/env python

import os
import sys
import re
import bisect

def usage():
    print "Usage: %s <file> [sample limit]" % sys.argv[0]
    print """
Print out useful information from a perf record run.
First run perf record -g, then run perf report -D > <file>.
This script parses the file created by that command.

Options:
Sample limit  -  The lower limit on the number of samples within
                 a function to appear in the listing. Set this to
                 ignore functions that were only sampled a few times.
                 (Default is 30)
"""

if len(sys.argv) < 2:
    usage()
    exit(1)

if len(sys.argv) > 2:
    filterlimit = int(sys.argv[2])
else:
    filterlimit = 30

filename = sys.argv[1]

contents = open(filename).read().split("\n")

allsyms = open("/proc/kallsyms").read().split("\n")

symsmap = dict()

for line in allsyms:
    if line == '':
        continue

    match = re.match("([0-9a-f]+).(.).(.*)", line)
    if not match:
        continue

    val = int(match.group(1), 16)
    name = match.group(3)

    name = re.sub(r'\s+', ' ', name)

    symsmap[val] = (name, match.group(2))


symkeys = sorted(symsmap.keys())

def findfnname(ip):
    global symkeys
    global symsmap

    index = bisect.bisect_right(symkeys, ip) - 1


    (key, t) = symsmap[symkeys[index]]
    if t != 't' and t != 'T':
        return "<unknown@%08x>" % ip
    else:
        return key


fncounts = dict()
btmap = dict()

for ind in range(len(contents)):

    line = contents[ind]

    match = re.findall(r'PERF_RECORD_SAMPLE[^x]*x([0-9a-f]+)', line)
    if match:
        ip = int(match[0], 16)

        key = findfnname(ip)



        if key in fncounts:
            old = fncounts[key]
            fncounts[key] = old + [ip]
        else:
            fncounts[key] = [ip]

        match = re.findall(r'chain: nr:(\d+)', contents[ind+1])
        if match:
            btlist = []
            items = int(match[0])
            for i in range(2, items):
                m = re.findall(r'\d+:.([0-9a-f]+)', contents[ind+2+i])
                btlist.append(m[0])

            btlist = map(lambda x: findfnname(int(x, 16)) + " at " + x, btlist)

            btlist = btlist[:5]

            btstr = ' // '.join(btlist)

            if key in btmap:
                d = btmap[key]
                if btstr in d:
                    d[btstr] = d[btstr] + 1
                else:
                    d[btstr] = 1
            else:
                d = dict()
                d[btstr] = 1
                btmap[key] = d
        

        #print "Have %8d for %s (its ip is %x mine is %x)" % (len(fncounts[key]), key, findkeyless(ip, symkeys), ip)

output = sorted(fncounts.iteritems(), key=lambda x: len(x[1]), reverse=True) 

output = filter(lambda x: len(x[1]) >= filterlimit, output)


def printoutput(output):
    i = 1
    for (name, lst) in output:
        print "[%3d] %-42s %9d" % (i, name, len(lst))
        i += 1


def read_symbol(index, output):
    global btmap

    if index < 1 or index > len(output):
        return
    (name, lst) = output[index-1]
    ipdict = dict()
    for x in lst:
        if x in ipdict:
            ipdict[x] = ipdict[x] + 1
        else:
            ipdict[x] = 1

    print "\nInstruction Counts:"
    for (x,y) in sorted(ipdict.iteritems(), key=lambda x: x[0]):
        print "0x%08x %10d" % (x,y)

    if not name in btmap:
        print "\nNo backtrace info!\n"
        return

    d = btmap[name]
    bts = sorted(d.iteritems(), key=lambda x: x[1], reverse=True)

    print "\nBacktraces:"

    for (string, count) in bts:
        print "%d times:\n%s\n" % (count, string)



printoutput(output)

while True:
    index = raw_input("Enter a number to output function info: ")
    if not index:
        exit(0)
    read_symbol(int(index), output)
    raw_input("Press enter to continue.")
    printoutput(output)


