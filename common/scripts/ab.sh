#!/bin/bash

#echo "Usage: $0 <processes> <connections> <time> <address>"

mkdir -p /tmp/$(whoami)
cd /tmp/$(whoami)

trap "{ rm -f ab.tmp.*; }" EXIT

for i in $(seq 1 ${1})
do
    /usr/sbin/ab -c $2 -t $3 -n1000000 http://$4/mem4 > ab.tmp.${i} 2>&1  &
done

wait

cat ab.tmp.* | grep Requests | awk '{ print $4 }'
exit 0

cat ab.tmp.* | grep Requests | awk '{ print $4 }' > ab.tmp.total
total=`cat ab.tmp.total | paste -sd+ | bc`
echo $total
