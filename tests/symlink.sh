#!/bin/bash
source `dirname $0`/checks.sh
start_test

make_dir directory
ln -s link00000.aa directory
link_exist directory/link00000.aa

remove_file directory/link00000.aa
remove_dir directory

make_dir d1
ln -s d1 l1
link_exist l1

readlink l1 > l1dest
echo "d1" > l1dest_right
files_same l1dest l1dest_right
remove_file l1dest
remove_file l1dest_right

remove_file l1
remove_dir d1

end_test
