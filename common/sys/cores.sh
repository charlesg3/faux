#!/bin/bash

# Input parsing

if [ $# -ne 1 ]
then
    echo Sets the number of active cores on the system.
    echo Usage: $0 ncores
    exit -1
fi

nactive=$(expr $1 - 1)

if [ $nactive -lt 0 ]
then
    echo Must have at least one active core!
    exit -2
fi

ncores=$(ls /sys/devices/system/cpu/ | grep cpu | wc -l)
ncores=$(expr $ncores - 2) # remove cpufreq cpuidle, base 0

if [ $nactive -gt $ncores ]
then
    echo "Asked for $nactive cores; only $ncores available."
    exit -3
fi

# Actual work

for c in $(seq 1 $nactive)
do
    status=$(cat /sys/devices/system/cpu/cpu${c}/online)
    if [ ${status} -ne 1 ]
    then
        echo 1 > /sys/devices/system/cpu/cpu${c}/online
    fi
done

nactive=$(expr $nactive + 1)

for c in $(seq $nactive $ncores)
do
    status=$(cat /sys/devices/system/cpu/cpu${c}/online)
    if [ ${status} -ne 0 ]
    then
        echo 0 > /sys/devices/system/cpu/cpu${c}/online
    fi
done
