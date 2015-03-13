#!/bin/bash

if [ $# -ne 1 ]
then
    echo List affinity of interrupts.
    echo Usage: $0 device
    exit -1
fi

interrupts=$(cat /proc/interrupts | grep ${1}- | awk '{ print $1 }' | cut -d: -f1)

tmp=/tmp/irq_affinity_list
rm -f ${tmp}*

for i in ${interrupts}
do
    dir=/proc/irq/$i
    if [ -e ${dir}/smp_affinity_list ]
    then
	affinity=$(cat ${dir}/smp_affinity_list)
    else
	affinity=$(cat ${dir}/smp_affinity)
    fi

    echo "$affinity $i" >> ${tmp}
done

if [ ! -e ${tmp} ]
then
    echo "No interrupts for $1 found."
    exit -1
fi

affinities=$(cat ${tmp} | awk '{ print $1 }' | sort -n | uniq)

for a in ${affinities}
do
    affs=$(awk "{ if (\$1==\"${a}\") printf \"%-6s \", \$2; }" ${tmp}) # > ${tmp}.${a}
    printf "%-6s ${affs}\n" ${a}:
done
