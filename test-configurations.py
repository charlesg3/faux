#!/bin/python

import subprocess
import os
import sys

options = [
"CHANNEL_USE_SCRATCH",
"CHANNEL_USE_SSE",
"CHANNEL_PULL_INTO_CACHE_WRITE",
"CHANNEL_PULL_INTO_CACHE_CMPXCHG",
"CHANNEL_PREFETCH",
"CHANNEL_MFENCE_SEND",
"CHANNEL_NO_ALLOCATOR"
]

confs = [
"ooooooo",
"ooxoooo",
"oooxooo",
"xoooooo",
"xoxoooo",
"xooxooo",
"xxooooo",
"xxxoooo",
"xxoxooo",
"xxxoxoo",
"xxoxxoo",
"ooooxoo",
"xxxooxo",
"xxoxoxo",
"oooooxo",
"xxxoxxo",
"xxoxxoo",
"oooooox",
"xooooox",
"xxoooox",
"xxxooox",
"xxoxoox"
]

for i in confs:
    for j in range(len(i)):
        if (i[j] == 'x'):
            sys.stdout.write('x\t')
        else:
            sys.stdout.write('\t')
    sys.stdout.write('\t')
    sys.stdout.flush()

    CFLAGS = ""
    for j in range(len(i)):
        if (i[j] == 'x'):
            CFLAGS += "-D" + options[j] + " "
    env = os.environ
    env["CFLAGS"] = CFLAGS
    subprocess.Popen(args=[],executable="./test-channels.sh", shell=True, env=env).wait()
