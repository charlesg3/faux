#!/bin/bash

if [ $# -ne 0 ]
then
    echo Lists the active cores on the system.
    echo Usage: $0
    exit -1
fi

ncores=$(ls /sys/devices/system/cpu/ | grep cpu | wc -l)
ncores=$(expr $ncores - 3) # remove cpufreq cpuidle, base 0

echo -n "0 " # 0 is always active

for c in $(seq 1 $ncores)
do
    if [ $(cat /sys/devices/system/cpu/cpu${c}/online) -eq "1" ]
    then
	echo -n "$c "
    fi
done

echo
