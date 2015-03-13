#!/bin/bash
client="LD_LIBRARY_PATH=~harshad/install/lib ~/research/faux/nickolecho/echo_client"
./common/scripts/mapreduce.sh $1 "${client} $3 4321 $2" "awk '{print \$1}' | paste -sd+ | bc"
