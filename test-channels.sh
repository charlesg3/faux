#!/bin/sh

make clean > /dev/null
make > /dev/null

ON=`build/test/channel 4 8`
ON_BIG=`build/test/channel_big 4 8`
ON_MULTI=`build/test/channel_multisend 8 0 4 8 16 20 24 28 32 36 40`
ON_MULTI_SHARED=`build/test/channel_multisend 8 1 4 8 16 20 24 28 32 36 40`
OFF=`build/test/channel 5 8`
OFF_BIG=`build/test/channel_big 5 8`
OFF_MULTI=`build/test/channel_multisend 8 0 5 8 16 20 24 28 32 36 40`
OFF_MULTI_SHARED=`build/test/channel_multisend 8 1 5 8 16 20 24 28 32 36 40`

echo "$ON\t$ON_BIG\t$ON_MULTI\t$ON_MULTI_SHARED\t$OFF\t$OFF_BIG\t$OFF_MULTI\t$OFF_MULTI_SHARED"
