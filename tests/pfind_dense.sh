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

$HARE_ROOT/build/tests/createdense || fail

#touch /stats
#rm /stats

start_ns="$(date +%s%N)"
$HARE_ROOT/build/tests/pfind || fail
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"
printf "$testname time to run: %d.%.3d sec\n" $sec $subms

#touch /stats
#cat /stats > $HARE_ROOT/graphs/data/$testname.ops.data

$HARE_ROOT/build/tests/rmdense || fail

if [[ "$MODE" == *local* ]]; then
	cd $startdir
	remove_dir $outdir
fi


end_test

