#!/bin/bash
source `dirname $0`/checks.sh
source `dirname $0`/progress.sh
start_test
getcpus

# copy over the manpages
make_dir man

SERVER_COUNT=`$HARE_ROOT/common/scripts/getconfig.sh SERVER_COUNT`

copy_clients=$[ 40 ]

echo "Copying man pages over..."
start_ns="$(date +%s%N)"
find /usr/share/man/ -name "*.gz" -type f | xargs -P $copy_clients -n 10 cp -f -v -t man | progress_by_line 3981
echo
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"
printf "$testname copy time: %d.%.3d sec\n" $sec $subms

rm -fr man

end_test

