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

# copy over the manpages
start_ns="$(date +%s%N)"
cp -r /usr/share/man .
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"
printf "$testname copy time: %d.%.3d sec\n" $sec $subms

mv man man.bak
mkdir man
cp -r man.bak man/man001
cp -r man.bak man/man002
cp -r man.bak man/man003
cp -r man.bak man/man004
cp -r man.bak man/man005
cp -r man.bak man/man006
cp -r man.bak man/man007
cp -r man.bak man/man008
cp -r man.bak man/man009
cp -r man.bak man/man010
cp -r man.bak man/man011
cp -r man.bak man/man012
cp -r man.bak man/man013
cp -r man.bak man/man014
cp -r man.bak man/man015
cp -r man.bak man/man016
cp -r man.bak man/man017
cp -r man.bak man/man018
cp -r man.bak man/man019
mv man.bak man

echo "Punzipping..."
rm -f /stats

start_ns="$(date +%s%N)"
find man -name "*.gz" -type f > files
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"
printf "$testname find time: %d.%.3d sec\n" $sec $subms

# unzip
start_ns="$(date +%s%N)"
xargs -a files -n 1000 -P $cpucount gunzip
elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"

file_count=`find man -type f | wc -l`

printf "$testname time to unzip: %d.%.3d sec, %d files\n" $sec $subms $file_count

rm -fr man
rm files

if [[ "$MODE" == *local* ]]; then
	cd $startdir
	remove_dir $outdir
fi

end_test

