#!/bin/bash
source `dirname $0`/checks.sh
start_test
getcpus

# if we're running 'locally' then use a ramdisk (in /dev/shm)
if [[ "$MODE" == *local* ]]; then
if [[ "$MODE" == *unfs* ]]; then
    outdir=/home/cg3/hare/unfs/$testname
else
    outdir=/dev/shm/mail
fi
else
    outdir=mail
fi
mkdir $outdir

export PATH=$PATH:$HARE_BUILD/tests/mail

#rm -f /stats
start_ns="$(date +%s%N)"
taskset -c 0 $HARE_BUILD/tests/mail/bench -a none $outdir $cpucount > output
#$HARE_BUILD/tests/mail/bench -a none $outdir $cpucount > output
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"
out=`cat output | grep secs`
rm output
printf "$testname time to mail: %s\n" "$out"

rm -fr $outdir
rm -fr /var/run/shm/notify

end_test
