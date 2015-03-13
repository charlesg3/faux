#!/bin/bash
source `dirname $0`/checks.sh
start_test

mkdir /tmp

echo "hello" > fappend
echo "world" >> fappend
echo "world" > fappend2
files_diff fappend fappend2
rm fappend fappend2
file_nexist fappend
file_nexist fappend2

echo 0 > f &&
cat >> f  <<_ &&
1
_
cat >> f  <<_ &&
2
_
cat >> f <<_ &&
3
_
cat < f > f2
matches "`cat f`" "0\n1\n2\n3\n"
remove_file f
remove_file f2

echo 0 > f3
echo 1 >> f3
echo 2 >> f3
echo 3 >> f3

matches "`cat f3`" "0\n1\n2\n3\n"
remove_file f3

end_test
