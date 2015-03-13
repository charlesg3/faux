#!/bin/bash

HARE_ROOT=`pwd`

declare -a tests=( fsstress punzip build_linux rm_dense rm_sparse writes copy renames creates directories )

for t in ${tests[@]} ; do
    make cleanup $t THREADS=20
    TESTNAME=$t make copy_stats
done

rm /var/run/shm/notify
make cleanup mailbench THREADS=12
TESTNAME=mailbench make copy_stats

make cleanup psearchy THREADS=8
TESTNAME=psearchy make copy_stats

make cleanup pfind_dense THREADS=20

make cleanup pfind_sparse THREADS=20

./graphs/opsplot.sh

make $HARE_ROOT/build/graphs/ops.svg


