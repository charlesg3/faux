#!/bin/bash

if [ $# -ne 1 ]
then
    filter="eth"
else
    filter=$1
fi

columns=`tput cols`

watch -d "cat /proc/interrupts | awk '{ printf \"%s\", \$NF; print }' | grep ${filter} | cut -c1-${columns}"
