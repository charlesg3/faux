#!/bin/bash
source `dirname $0`/checks.sh
source `dirname $0`/progress.sh

# if we're running 'locally' then
# use a ramdisk (in /dev/shm)
outdir=./fss
if [[ "$MODE" == *local* ]]; then
    outdir=/dev/shm/pfind
    mkdir -p /dev/shm/pfind
    cd /dev/shm/pfind
fi

start_test

version=3.9
kernel=linux-$version
file=$kernel.tar.xz
src=http://www.kernel.org/pub/linux/kernel/v3.x
dest=$HARE_CONTRIB

# first make sure we have a copy of the source in hare
if [ ! -e $dest/$file ]; then
    echo "Downloading $file..."
    wget -P $dest $src/$file
fi

# extract the kernel in to hare
if [ ! -d $kernel ]; then
    echo "Extracting $kernel"
    start_ns="$(date +%s%N)"
    tar -xvf $dest/$file | progress_by_line 45168
    elapsed_ns="$(($(date +%s%N)-start_ns))"
    sec="$((elapsed_ns/1000000000))"
    subms="$((elapsed_ns/1000000 - (sec * 1000)))"
    echo
    printf "Extract took: %d.%.3d sec\n" $sec $subms
fi

if [ ! -d man ]
then
make_dir man
cp /usr/share/man/man2/* man
fi

findargs="-type f -maxdepth 1"
argcount=5 # 4 args above plus the path

rm -f /stats

getcpus
start_ns="$(date +%s%N)"
file_count=`find . -type d -print | awk "{ print \\\$0, \"$findargs\" }" | xargs -n $argcount -P $cpucount find`
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"
printf "$testname time to find: %d.%.3d sec\n" $sec $subms

rm -fr man
rm -fr $kernel

end_test
