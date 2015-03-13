#!/usr/bin/env python

import sys
import os

data=[]
cores={}

class Block:
    def __init__(self, block, access_time):
        self.block = block
        self.access_time = access_time;

class BufferCache:
    def __init__(self):
        self.inodes = {}
        self.misses = 0

    def access(self, inode, block, time):
        inodes = self.inodes
        if inode in inodes:
            block_list = inodes[inode]
            for b in block_list:
                if b.block == block:
                    b.access_time = time
                    block_list.insert(0, block_list.pop(block_list.index(b))) # move to front
                    return
            # didn't find this block in the inode list, add it to the front
            self.misses += 1
            block_list.insert(0, Block(block, time))
        else:
            # didn't find the block...
            inodes[inode] = [Block(block, time)]
            self.misses += 1

    def delete(self, inode):
        if inode in self.inodes: del self.inodes[inode]

def process_line(line):
    parts = line.strip().split(" ")
    if len(parts) == 5:
        (time, op, core, inode, block) = parts
        data.append((int(time),op,int(core),int(inode),int(block)))

def process_file(path):
    f = open(path, 'r')
    for line in f.readlines():
        process_line(line)

def simulate_entry(time, op, core, inode, block):
    #if(op == "ACCESS"): cores[int(core)].access(inode, block, time)
    if(op == "ACCESS"): cores[0].access(inode, block, time)
    if(op == "DELETE"):
        cores[0].delete(inode)
#        for c in cores:
#            cores[c].delete(inode)

def simulate_full(data):
    for point in data:
        simulate_entry(int(point[0]), point[1], int(point[2]), int(point[3]), int(point[4]))

bcl_dir = sys.argv[1]
files = os.listdir(bcl_dir)

print "processing files"

for f in files:
    fullname = os.path.join(bcl_dir,f)
    process_file(fullname)

print "sorting"

data_sorted = sorted(data, key=lambda line: line[0])

for i in range(1):
    cores[i] = BufferCache()

print "simulating"

simulate_full(data_sorted)

for i in range(1):
    print "core: ", i, "misses: ", cores[i].misses

total = 0
for i in range(1):
    total += cores[i].misses

print "total: ", total
