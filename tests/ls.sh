#!/bin/bash
source `dirname $0`/checks.sh
start_test

umask 022

make_dir d1
make_dir d2
touch f1
make_file f2 "hello world."
make_file d1/f1 "hello world."
ln -s d1/f1 s1
ln -s /does/not/exist s2

ls -la --time-style=+"" | grep -v total > /$testname.out

files_same /$testname.out $HARE_ROOT/tests/files/ls.expected
remove_file /$testname.out

remove_file d1/f1
remove_file f2
remove_file s1
remove_file s2
remove_dir d1
remove_dir d2
remove_file f1

end_test

