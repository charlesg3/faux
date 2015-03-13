#!/bin/bash
source `dirname $0`/checks.sh
start_test

mkdir -p d/d/d/d
make_file d/d/d/d/f
rm -rf d
dir_nexist d

end_test
