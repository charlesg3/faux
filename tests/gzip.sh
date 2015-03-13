#!/bin/bash
source `dirname $0`/checks.sh
start_test

make_file f1 "hello world."

gzip f1
file_exist f1.gz
gunzip f1.gz
file_exist f1

matches "`cat f1`" "hello world.\n"

cat f1 | gzip - > f1.gz
file_exist f1.gz
cat f1.gz | gunzip - > f1
remove_file f1.gz

file_exist f1

matches "`cat f1`" "hello world.\n"

remove_file f1

end_test
