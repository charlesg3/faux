#!/bin/bash
source `dirname $0`/checks.sh
source `dirname $0`/progress.sh
start_test
getcpus

# if we're running 'locally' then use a ramdisk (in /dev/shm)
if [[ "$MODE" == *local* ]]; then
	startdir=`pwd`
if [[ "$MODE" == *unfs* ]]; then
    outdir=/home/cg3/hare/unfs/$testname
else
    outdir=/var/run/shm/$testname
fi
	mkdir $outdir
	cd $outdir
fi

version=3.9
kernel=linux-$version
file=$kernel.tar.xz
src=http://www.kernel.org/pub/linux/kernel/v3.x
dest=$HARE_CONTRIB

mkdir -p /tmp

# download the kernel to host fs
if [ ! -e $dest/$file ]; then
    echo "Downloading $file..."
    wget -P $dest $src/$file
fi

start_ns="$(date +%s%N)"
tar -xf $dest/$file
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"
echo
printf "$testname time to run: %d.%.3d sec\n" $sec $subms

rm -fr $kernel

if [[ "$MODE" == *local* ]]; then
	cd $startdir
	remove_dir $outdir
fi

end_test

