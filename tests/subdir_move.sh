#!/bin/bash

SCRIPT_DIR=`dirname $0`
source $SCRIPT_DIR/checks.sh

start_test

mkdir d1
dir_exist d1
mkdir d1/d2
dir_exist d1/d2
touch d1/d2/f1 || fail
file_exist d1/d2/f1
cd d1 || fail

mv d2/f1 d2/f2 || fail
file_exist ../d1/d2/f2

mv d2 d3 || fail
file_exist ../d1/d3/f2
mv d3 d2 || fail

mv d2/f2 ../f3 || fail
cd ..
file_exist f3

cd d1

mv ../f3 d2
file_exist d2/f3

mv d2/f3 d2/f2
file_exist d2/f2

remove_file d2/f2
remove_dir d2
cd ..
remove_dir d1

end_test

