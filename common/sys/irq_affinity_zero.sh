#!/bin/bash

if [ $# -ne 1 ]
then
    echo Stripe interrupts over cores.
    echo Usage: $0 device
    exit -1
fi

killall irqbalance 2> /dev/null

interrupts=`cat /proc/interrupts | grep ${1}- | awk '{ print \$1 }' | cut -d: -f1`
min=`echo $interrupts | awk '{ print \$1 }'`

for i in ${interrupts}
do
    echo 0 > /proc/irq/$i/smp_affinity_list
done
