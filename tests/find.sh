#!/bin/bash
source `dirname $0`/checks.sh
start_test

make_dir d1
make_dir d2
touch f1
make_file f2 "hello world."
make_file d1/f1 "hello world."
ln -s d1/f1 s1
ln -s /does/not/exist s2

matches "`find / | grep \"/find\" | sort`" "/find\n/find/d1\n/find/d1/f1\n/find/d2\n/find/f1\n/find/f2\n/find/s1\n/find/s2\n"

remove_file d1/f1
remove_file f2
remove_file s1
remove_file s2
remove_file f1
remove_dir d1
remove_dir d2

end_test

