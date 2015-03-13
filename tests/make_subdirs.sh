#!/bin/bash
source `dirname $0`/checks.sh
start_test

make_dir d1
make_dir d1/d2
make_dir d1/d3

remove_dir d1/d2
remove_dir d1/d3
remove_dir d1

mkdir -p d4/d5
dir_exist d4/d5
rm -r d4
dir_nexist d4

make_dir d1
mv d1 d1/d2 2> /dev/null && fail

dir_exist d1
remove_dir d1

make_dir d1/
remove_dir d1/

dir=abs$$

make_dir /$dir
make_dir /$dir/d
make_dir /$dir
dir_exist /$dir/d

make_dir /$dir/d/a
dir_exist /$dir/d/a
make_dir /$dir/d/b
dir_exist /$dir/d/a
dir_exist /$dir/d/b
make_dir /$dir/d/c
dir_exist /$dir/d/a
dir_exist /$dir/d/b
dir_exist /$dir/d/c

remove_dir /$dir/d/a
remove_dir /$dir/d/b
remove_dir /$dir/d/c
remove_dir /$dir/d
remove_dir /$dir

mkdir d
/bin/sh -c "ls d/*" 2>/dev/null && fail
remove_dir d

end_test
