#!/bin/bash
source `dirname $0`/checks.sh
start_test

cp $HARE_ROOT/tests/files/script.sh .
cp $HARE_ROOT/tests/files/makefile .

make --no-print-directory script

$HARE_CONTRIB/make-dfsg-3.81/make --no-print-directory parallel -j 2

matches "`make --no-print-directory`" "hi\n"
remove_file makefile
remove_file script.sh

end_test
