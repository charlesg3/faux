#!/bin/bash
source `dirname $0`/checks.sh
start_test

make_file f1 asdf
mv f1 f2
file_exist f2
file_nexist f1
remove_file f2

make_dir d
make_file f
cd d

mv /$testname/f /$testname/f2

file_exist /$testname/f2
file_nexist /$testname/f

cd ..
remove_dir d
remove_file f2

end_test

