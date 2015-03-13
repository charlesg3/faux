#!/bin/bash
source `dirname $0`/checks.sh
start_test

echo 'hi' | cat - > pipedhi
file_exist pipedhi
echo 'hi' > hi
file_exist hi
files_same pipedhi hi

remove_file pipedhi
remove_file hi

end_test
