#!/bin/bash

if [ $# -ne 2 ]
then
    echo Stripe interrupts over the first X cores.
    echo Usage: $0 device ncores
    exit -1
fi

killall irqbalance 2> /dev/null

interrupts=`cat /proc/interrupts | grep ${1}- | awk '{ print \$1 }' | cut -d: -f1`
min=`echo $interrupts | awk '{ print \$1 }'`

for i in ${interrupts}
do
    core=`expr \( $i - $min \) % $2`
    echo $core > /proc/irq/$i/smp_affinity_list
done
