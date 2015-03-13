#!/bin/bash
./common/scripts/mapreduce.sh $1 "~/research/faux/src/common/scripts/ab.sh $2 $3 10 $4" "paste -sd+ | bc"
