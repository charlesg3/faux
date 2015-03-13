#!/bin/bash
source `dirname $0`/checks.sh
start_test
getcpus

# if we're running 'locally' then use a ramdisk (in /dev/shm)
outdir=.
if [[ "$MODE" == *local* ]]; then
    #outdir=/dev/shm/mail
    #mkdir $outdir
    outdir=.
fi

rm -f /stats
start_ns="$(date +%s%N)"
out=`$HARE_BUILD/tests/mail/bench -a none $outdir $cpucount | grep "secs"`
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"
printf "$testname time to mail: %s\n" "$out"

rm -fr $outdir/*
rm -fr /var/run/shm/notify

end_test
