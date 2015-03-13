#!/bin/bash
source `dirname $0`/checks.sh
source `dirname $0`/progress.sh
start_test

mkdir -p /tmp

ver=dfsg-3.81
dir=make-$ver

echo "Copying from host..."
if [ ! -d $dir ]; then
  cp -rv $HARE_CONTRIB/$dir . | progress_by_line 435
  echo
  dir_exist $dir
  awk '/srcdir = /{print "datarootdir = @datarootdir@"}{print $0}' $dir/po/Makefile.in.in > $dir/po/Makefile.in.in.edit
  file_exist $dir/po/Makefile.in.in.edit
  mv $dir/po/Makefile.in.in.edit $dir/po/Makefile.in.in
fi

cd $dir

touch po/stamp-po

# remove the object files and binary if they exist
find . -name "*.o" | xargs rm -f
rm -f make

# will store the number of cpus in $cpucount
getcpus

echo "Building..."
start_ns="$(date +%s%N)"
make -j $cpucount 2>&1 | progress_by_line 90
echo
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
subms="$((elapsed_ns/1000000 - (sec * 1000)))"
printf "Build took: %d.%.3d sec\n" $sec $subms

file_exist make

cd ..

rm -rf $dir

end_test
