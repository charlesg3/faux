#!/bin/bash
source `dirname $0`/checks.sh
start_test

make_file f1 "hello world."
make_file f2 "hello world."
files_same f1 f2

echo -n "hello" > f2
files_diff f1 f2

remove_file f1
remove_file f2

make_file f "hello world."
truncate -s 5 f

matches "`cat f`" "hello"

truncate -s 64 f
matches "`wc -c ./f | cut -d ' ' -f 1`" "64"

truncate -s 4096 f
matches "`wc -c ./f | cut -d ' ' -f 1`" "4096"

truncate -s 1048576 f
matches "`wc -c ./f | cut -d ' ' -f 1`" "1048576"

truncate -s 65535 f
matches "`wc -c ./f | cut -d ' ' -f 1`" "65535"

truncate -s 1 f
matches "`wc -c ./f | cut -d ' ' -f 1`" "1"

truncate -s 0 f
matches "`wc -c ./f | cut -d ' ' -f 1`" "0"

remove_file f

end_test
