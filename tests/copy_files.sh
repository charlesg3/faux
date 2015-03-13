#!/bin/bash
source `dirname $0`/checks.sh
start_test

make_file f1 "hello world."
cp f1 f2
cp f1 f3
files_same f1 f2
files_same f1 f3

mv f1 f2
file_nexist f1
file_exist f2
files_same f2 f3

cp f2 f1
files_same f1 f2
cp f1 f2
files_same f1 f2

remove_file f1
remove_file f2
remove_file f3

make_file f1 "hello world."
make_file f2 "hello"
cp f2 f1

matches "`cat f1`" "hello\n"

remove_file f1
remove_file f2

make_file f
truncate -s 65535 f

# arg1 is src
# arg2 is dest
function copy_check {
if [ -e $2 ]; then
cp $1 $2
file_exist $2/$1
remove_file $2/$1
file_nexist $2/$1
else
cp $1 $2
file_exist $2
remove_file $2
file_nexist $2
fi
}

#file in subdirs
make_dir d
copy_check f d

make_dir d/d2
copy_check f d/d2
remove_dir d/d2
remove_dir d

dir=abs$$
make_dir /$dir
make_dir /$dir/d
copy_check f /$dir
copy_check f /$dir/f2
copy_check f /$dir/d

remove_dir /$dir/d
remove_dir /$dir

remove_file f

end_test
