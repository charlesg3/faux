#!/bin/bash

if [ $# -ne 2 ]
then
    echo "Usage: $0 (good|bad) num_pairs"
    exit -1
fi

good="0 4 1 5 2 6 3 7 8 12 9 13 10 14 11 15 16 20 17 21 18 22 19 23 24 28 25 29 26 30 27 31 32 36 33 37 34 38 35 39"
bad="0 1 2 3 4 6 5 7 8 11 9 10 12 13 14 15 16 18 17 19 20 23 21 22 24 25 26 27 28 30 29 31 32 35 33 34 36 37 38 39"

if [ "$1" = "good" ]
then
    order=$good
elif [ "$1" = "bad" ]
then
    order=$bad
else
    echo "Usage: $0 (good|bad) num_pairs"
    exit -1
fi

num_pairs=`expr $2 \* 2`
param=`echo $order | cut -d\  -f1-$num_pairs`

./build/test/channel_concurrent $param
