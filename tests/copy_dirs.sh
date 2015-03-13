#!/bin/bash
source `dirname $0`/checks.sh
start_test

make_dir d
make_dir d/a
make_dir d/a/b
make_dir d/b
make_file d/f

# arg1 is src
# arg2 is dest
function copy_check {
if [ -e $2 ]; then
cp -r $1 $2
dir_exist $2/d
dir_exist $2/d/a
dir_exist $2/d/a/b
dir_exist $2/d/b
file_exist $2/d/f
rm -fr $2/d
else
cp -r $1 $2
dir_exist $2
dir_exist $2/a
dir_exist $2/a/b
dir_exist $2/b
file_exist $2/f
rm -fr $2
fi
dir_exist $1
dir_exist $1/a
dir_exist $1/a/b
dir_exist $1/b
file_exist $1/f
}

# relative -> subdir
copy_check d d2

# relative subdir exists
make_dir d2
copy_check d d2

# relative subdir/subdir exists
mkdir d2/d3
copy_check d d2/d3
rm -fr d2/d3
remove_dir d2

dir=abs$$
make_dir /$dir
copy_check d /$dir

# relative -> absolute subdir
copy_check d /$dir/d
remove_dir /$dir

cp -r d /d2
copy_check /d2 c

rm -fr /d2
rm -fr d

end_test
