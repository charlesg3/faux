#!/bin/bash
source `dirname $0`/checks.sh
source `dirname $0`/progress.sh

start_test

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

# configure the kernel
if [ ! -e $kernel/.config ]; then
    echo "Configuring the kernel ($kernel)"
    /home/cg3/hare-contrib/make-dfsg-3.81/make -C $kernel defconfig | progress_by_line 13
    echo
fi

rm -fr $kernel/Documentation
rm -fr $kernel/arch/alpha
rm -fr $kernel/arch/arc
rm -fr $kernel/arch/arm
rm -fr $kernel/arch/arm64
rm -fr $kernel/arch/avr32
rm -fr $kernel/arch/blackfin
rm -fr $kernel/arch/c6x
rm -fr $kernel/arch/cris
rm -fr $kernel/arch/frv
rm -fr $kernel/arch/h8300
rm -fr $kernel/arch/hexagon
rm -fr $kernel/arch/ia64
rm -fr $kernel/arch/m32r
rm -fr $kernel/arch/68k
rm -fr $kernel/arch/metag
rm -fr $kernel/arch/microblaze
rm -fr $kernel/arch/mips
rm -fr $kernel/arch/mn10300
rm -fr $kernel/arch/openrisc
rm -fr $kernel/arch/parisc
rm -fr $kernel/arch/powerpc
rm -fr $kernel/arch/s390
rm -fr $kernel/arch/score
rm -fr $kernel/arch/sh
rm -fr $kernel/arch/sparc
rm -fr $kernel/arch/tile
rm -fr $kernel/arch/um
rm -fr $kernel/arch/unicore32
rm -fr $kernel/arch/xtensa
find $kernel/samples -name "*.c" | xargs rm
find $kernel/firmware -name "*.ihex" | egrep -v "e100|tigon" | xargs rm
find $kernel/firmware -name "*.HEX" | egrep -v "e100|tigon" | xargs rm

find $kernel/fs/9p -name "*.c" | xargs rm
find $kernel/fs/9p -name "*.h" | xargs rm
find $kernel/fs/adfs -name "*.c" | xargs rm
find $kernel/fs/adfs -name "*.h" | xargs rm
find $kernel/fs/affs -name "*.c" | xargs rm
find $kernel/fs/affs -name "*.h" | xargs rm
find $kernel/fs/befs -name "*.c" | xargs rm
find $kernel/fs/befs -name "*.h" | xargs rm
find $kernel/fs/bfs -name "*.c" | xargs rm
find $kernel/fs/bfs -name "*.h" | xargs rm
find $kernel/fs/btrfs -name "*.c" | xargs rm
find $kernel/fs/btrfs -name "*.h" | xargs rm
find $kernel/fs/ceph -name "*.c" | xargs rm
find $kernel/fs/ceph -name "*.h" | xargs rm
find $kernel/fs/cifs -name "*.c" | xargs rm
find $kernel/fs/cifs -name "*.h" | xargs rm
find $kernel/fs/coda -name "*.c" | xargs rm
find $kernel/fs/coda -name "*.h" | xargs rm
find $kernel/fs/cramfs -name "*.c" | xargs rm
find $kernel/fs/ecryptfs -name "*.c" | xargs rm
find $kernel/fs/ecryptfs -name "*.h" | xargs rm
find $kernel/fs/efs -name "*.c" | xargs rm
find $kernel/fs/efs -name "*.h" | xargs rm
find $kernel/fs/exofs -name "*.c" | xargs rm
find $kernel/fs/exofs -name "*.h" | xargs rm
find $kernel/fs/exportfs -name "*.c" | xargs rm
find $kernel/fs/f2fs -name "*.c" | xargs rm
find $kernel/fs/f2fs -name "*.h" | xargs rm
find $kernel/fs/freevxfs -name "*.c" | xargs rm
find $kernel/fs/freevxfs -name "*.h" | xargs rm
find $kernel/fs/gfs2 -name "*.c" | xargs rm
find $kernel/fs/gfs2 -name "*.h" | xargs rm
find $kernel/fs/hfs -name "*.c" | xargs rm
find $kernel/fs/hfs -name "*.h" | xargs rm
find $kernel/fs/hfsplus -name "*.c" | xargs rm
find $kernel/fs/hfsplus -name "*.h" | xargs rm
find $kernel/fs/hostfs -name "*.c" | xargs rm
find $kernel/fs/hostfs -name "*.h" | xargs rm
find $kernel/fs/hpfs -name "*.c" | xargs rm
find $kernel/fs/hpfs -name "*.h" | xargs rm
find $kernel/fs/hppfs -name "*.c" | xargs rm
find $kernel/fs/jbd -name "*.c" | xargs rm
find $kernel/fs/jfs -name "*.c" | xargs rm
find $kernel/fs/jfs -name "*.h" | xargs rm
find $kernel/fs/minix -name "*.c" | xargs rm
find $kernel/fs/minix -name "*.h" | xargs rm
find $kernel/fs/ncpfs -name "*.c" | xargs rm
find $kernel/fs/ncpfs -name "*.h" | xargs rm
find $kernel/fs/nilfs2 -name "*.c" | xargs rm
find $kernel/fs/nilfs2 -name "*.h" | xargs rm
find $kernel/fs/ocfs2 -name "*.c" | xargs rm
find $kernel/fs/ocfs2 -name "*.h" | xargs rm
find $kernel/fs/omfs -name "*.c" | xargs rm
find $kernel/fs/omfs -name "*.h" | xargs rm
find $kernel/fs/openpromfs -name "*.c" | xargs rm
find $kernel/fs/qnx4 -name "*.c" | xargs rm
find $kernel/fs/qnx4 -name "*.h" | xargs rm
find $kernel/fs/qnx6 -name "*.c" | xargs rm
find $kernel/fs/qnx6 -name "*.h" | xargs rm
find $kernel/fs/ubifs -name "*.c" | xargs rm
find $kernel/fs/ubifs -name "*.h" | xargs rm
find $kernel/fs/ufs -name "*.c" | xargs rm
find $kernel/fs/ufs -name "*.h" | xargs rm
find $kernel/net/9p -name "*.c" | xargs rm
find $kernel/net/phonet -name "*.c" | xargs rm
find $kernel/sound/arm -name "*.c" | xargs rm
find $kernel/sound/parisc -name "*.c" | xargs rm
find $kernel/sound/soc -name "*.c" | xargs rm

sed -i 's/\(-fno-delete-null-pointer-checks\)/\1 -fno-stack-protector -Wno-pointer-sign -Wno-unused-but-set-variable -Wno-unused-local-typedefs/' ./$kernel/Makefile

# build the kernel
echo "Building $kernel"

rm -f /stats

getcpus
start_ns="$(date +%s%N)"

/home/cg3/hare-contrib/make-dfsg-3.81/make -C $kernel -j $cpucount all | progress_by_line 2568
echo

elapsed_ns="$(($(date +%s%N)-start_ns))"
sec="$((elapsed_ns/1000000000))"
ms="$((elapsed_ns/1000000))"
subms="$((ms - (sec * 1000)))"

file_exist $kernel/vmlinux
#matches "`nm $kernel/vmlinux.o | grep -v ".LC" | md5sum`" "cd1907328167f634559e24f50b479560  -"
matches "`nm $kernel/vmlinux.o | grep -v ".LC" | md5sum`" "789f8747d9a2085fc1c82ab76b533f85  -"

printf "$testname time to build: %d.%.3d sec\n" $sec $subms

rm -fr $kernel

if [[ "$MODE" == *local* ]]; then
	cd $startdir
	remove_dir $outdir
fi

end_test

