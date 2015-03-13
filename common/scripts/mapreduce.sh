#!/bin/bash

./common/scripts/lock_acquire.sh
trap '{ rm -f map.tmp.* reduce.tmp*; kill $(jobs -pr); ./common/scripts/lock_release.sh; }' EXIT

if [ $# -ne 3 ]
then
    echo "Usage: $0 <num machines> <map> <reduce>"
    exit -1
fi

machines=$(cat ./common/scripts/mapreduce.machines | head -$1)

rm -f map.tmp.* reduce.tmp*

for m in $machines
do
    (ssh -tt $m $2 > map.tmp.$m 2>&1) &
done

wait

echo $3 > reduce.tmp.sh
chmod +x reduce.tmp.sh

cat map.tmp.* > reduce.tmp
reduction=$(./reduce.tmp.sh < reduce.tmp)

echo Map \($(cat reduce.tmp | wc -w)\): $(cat reduce.tmp)
echo Reduce: $reduction
