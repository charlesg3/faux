#!/bin/bash
source `dirname $0`/checks.sh
start_test

mkdir -p d1/d2
remove_dir d1/d2
dir_nexist d1/d2
dir_exist d1
rm -r d1

end_test
