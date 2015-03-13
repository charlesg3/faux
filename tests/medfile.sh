#!/bin/bash
source `dirname $0`/checks.sh
start_test

cp $HARE_ROOT/tests/medfile .
ls -la > /dev/null
remove_file medfile

end_test
