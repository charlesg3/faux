#!/bin/bash
source `dirname $0`/checks.sh
source `dirname $0`/progress.sh
start_test

mkdir -p /tmp

version=3.82
dir=make-$version
file=$dir.tar.gz
src=http://ftp.gnu.org/gnu/make
host=$HARE_CONTRIB

# download the tar from gnu.org
echo "Downloading..."
if [ ! -e $file ]; then
  wget -q $src/$file
fi
file_exist $file

# extract 
echo "Extracting..."
if [ ! -d $dir ]; then
  tar -xvf $file | progress_by_line 361
  dir_exist $dir
  awk '/srcdir = /{print "datarootdir = @datarootdir@"}{print $0}' $dir/po/Makefile.in.in > $dir/po/Makefile.in.in.edit
  file_exist $dir/po/Makefile.in.in.edit
  mv $dir/po/Makefile.in.in.edit $dir/po/Makefile.in.in
  echo
fi

# configure
echo "Configuring..."
if [ ! -e $dir/build.sh ] ; then
  cd $dir
  ./configure 2>&1 | progress_by_line 163
  file_exist build.sh
  echo
  cd ..
fi

# make
echo "Building..."
cd $dir
start_ns="$(date +%s%N)"
./build.sh | progress_by_line 26
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
subms="$((elapsed_ns/1000000 - (sec * 1000)))"
echo
printf "Build took: %d.%.3d sec\n" $sec $subms
file_exist make
cd ..

# remove
remove_file $file
remove_dir $dir
remove_dir /tmp

end_test
