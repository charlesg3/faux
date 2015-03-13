#!/bin/bash
source `dirname $0`/checks.sh
start_test

# run fsstress with a defined number of processes and seed
$HARE_BUILD/tests/fsstress/fsstress -n 1000 -f dwrite=0 -f dread=0 -f mknod=0 -f fdatasync=0 -f link=0 -f sync=0 -f fsync=0 -f mknod=0 -f chown=0 -d ./fss -s 1367354955 -p 1

matches "`find ./fss | wc -l`" "404\n"

$HARE_BUILD/tests/fsstress/fsstress -n 1000 -f dwrite=0 -f dread=0 -f mknod=0 -f fdatasync=0 -f link=0 -f sync=0 -f fsync=0 -f mknod=0 -f chown=0 -d ./fss > /dev/null

rm -fr ./fss

end_test
