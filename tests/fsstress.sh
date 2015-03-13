#!/bin/bash
source `dirname $0`/checks.sh
start_test
getcpus

ops="16384"

# if we're running 'locally' then use a ramdisk (in /dev/shm)
outdir=./fss
if [[ "$MODE" == *local* ]]; then
if [[ "$MODE" == *unfs* ]]; then
    outdir=/home/cg3/hare/unfs/$testname
else
    outdir=/dev/shm/fss
fi
fi

rm -f /stats
start_ns=`date +%s%N`
$HARE_BUILD/tests/fsstress/fsstress -n $ops -f dwrite=0 -f dread=0 -f mknod=0 -f fdatasync=0 -f link=0 -f sync=0 -f fsync=0 -f mknod=0 -f truncate=0 -p $cpucount -d $outdir -s 1367354955 | grep -v seed
elapsed_ns=$[$(date +%s%N)-start_ns]
sec=$[elapsed_ns/1000000000]
ms=$[elapsed_ns/1000000]
subms=$[(ms - (sec * 1000))]
printf "$testname time to stress: %d.%.3d sec\n" $sec $subms

rm -fr $outdir

end_test
