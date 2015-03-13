#!/bin/sh

echo "copying stats into: $HARE_ROOT/graphs/data/$TESTNAME.ops.data"
touch /stats
cat /stats > $HARE_ROOT/graphs/data/$TESTNAME.ops.data

