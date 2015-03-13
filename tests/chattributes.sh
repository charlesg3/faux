#!/bin/bash
source `dirname $0`/checks.sh
start_test

make_file f1 asdf
chown 0:0 f1
matches `stat -c "%u:%g" f1` 0:0
chown 500:500 f1
matches `stat -c "%u:%g" f1` 500:500
chown 600 f1
matches `stat -c "%u:%g" f1` 600:500
chgrp 700 f1
matches `stat -c "%u:%g" f1` 600:700
remove_file f1

function chmod_any {
chmod 755 $1
matches `stat -c "%a" $1` 755
chmod 000 $1
matches `stat -c "%a" $1` 0
chmod a+x $1
matches `stat -c "%a" $1` 111
chmod g+r $1
matches `stat -c "%a" $1` 151
chmod a+r $1
matches `stat -c "%a" $1` 555
chmod a+w $1
chmod a-x $1
matches `stat -c "%a" $1` 666
}

function chmod_f {
make_file $1 asdf
path=$1
truncate -s 65535 $path
chmod 777 $path
matches `ls -la $path | awk '{ print $1 }'` "-rwxrwxrwx"
chmod_any $path
remove_file $path
}

function chmod_d {
make_dir $1
path=$1
chmod 777 $path
matches `ls -la $path | grep -v total | grep -e " .$" | awk '{ print $1 }'` "drwxrwxrwx"
chmod_any $path
remove_dir $path
}

chmod_f f
chmod_d d

dir=abs$$

make_dir /$dir
chmod_f /$dir/f
chmod_d /$dir/d
remove_dir /$dir

end_test
