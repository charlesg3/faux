#!/bin/bash
source `dirname $0`/checks.sh
start_test

# create a few entries for the tar files
mkdir d
touch d/f

# creat a tar file
tar -cf t.tar d || fail
file_exist t.tar

# extract tar file in subdir
mkdir d2
cd d2
tar -xf ../t.tar || fail
file_exist d/f
dir_exist d

# remove files / subdir
remove_file d/f
remove_dir d
cd ..
remove_dir d2
remove_file t.tar

# create a .tar.gz
tar -czf t.tar.gz d || fail

file_exist t.tar.gz

# extract in subdir
mkdir d2
cd d2
tar -zxf ../t.tar.gz
file_exist d/f
dir_exist d

# check / remove
remove_file d/f
remove_dir d
cd ..
remove_dir d2

remove_file t.tar.gz

umask 022

# check that perms are preserved over a couple tar/untars
chmod 755 d/f
chown 100:200 d/f
chown 300:300 d

tar --atime-preserve -czf t.tar.gz d || fail

# sleep if you really want to check atimes...
# sleep 1

mkdir d2
cd d2
tar --atime-preserve --same-owner --same-permissions -zxf ../t.tar.gz
tar --atime-preserve -czf ../t2.tar.gz d || fail
cd ..
remove_file d2/d/f
remove_dir d2/d
remove_dir d2

# sleep if you really want to check atimes...
# sleep 1

mkdir d2
cd d2
tar --atime-preserve --same-owner --same-permissions -zxf ../t2.tar.gz
tar --atime-preserve -czf ../t3.tar.gz d
cd ..
remove_file d2/d/f
remove_dir d2/d
remove_dir d2

matches "`tar --full-time -tvf t.tar.gz | md5sum | awk '{ print $1 }'`" "`tar --full-time -tvf t2.tar.gz | md5sum | awk '{ print $1 }'`"
matches "`tar --full-time -tvf t.tar.gz | md5sum | awk '{ print $1 }'`" "`tar --full-time -tvf t3.tar.gz | md5sum | awk '{ print $1 }'`"

tar -zxf t3.tar.gz
remove_file t.tar.gz
remove_file t2.tar.gz
remove_file t3.tar.gz

remove_file d/f
remove_dir d

end_test
