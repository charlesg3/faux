#!/usr/bin/python

import subprocess
import sys

print sys.argv[0]

output = subprocess.check_output(["ps", "aux"])
# Get the lines as a list of lists of nonempty strings
lines = map(lambda x: filter(lambda y: y != '', x), map(lambda x: x.split(" "), output.split("\n")))

testlines = filter(lambda y: len(y) > 1 and y[0] == "fauxtest", lines)

for line in testlines:
    if line[-1] == '-bash':
        print "Skipping shell ", line[1]
        continue
    if line[-1] == sys.argv[0]:
        print "Skipping self ", sys.argv[0]
        continue
    if line[-1][0:9] == "fauxtest@":
        print "Skipping ssh ", line[1]
        continue
    print "kill -9", line[1]
    subprocess.call(['kill', '-9', line[1]])
