#!/bin/bash
source `dirname $0`/checks.sh
start_test
getcpus

# if we're running 'locally' then use a ramdisk (in /dev/shm)
if [[ "$MODE" == *local* ]]; then
	startdir=`pwd`
if [[ "$MODE" == *unfs* ]]; then
    outdir=/home/cg3/hare/unfs/$testname
else
    outdir=/var/run/shm/$testname
fi
	mkdir $outdir
	cd $outdir
fi

$HARE_ROOT/build/tests/createsparse || fail

start_ns="$(date +%s%N)"
$HARE_ROOT/build/tests/rmsparse || fail
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"
printf "$testname time to run: %d.%.3d sec\n" $sec $subms


if [[ "$MODE" == *local* ]]; then
	cd $startdir
	remove_dir $outdir
fi


end_test

