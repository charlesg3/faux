#!/bin/bash
source `dirname $0`/checks.sh
start_test

mkdir -p /tmp

cp $HARE_ROOT/tests/files/helloworld.c .
gcc helloworld.c -pie -fPIE -o helloworld

file_exist helloworld

matches "`./helloworld`" "Hello, world!\n"

remove_file helloworld
remove_file helloworld.c

remove_dir /tmp

end_test

