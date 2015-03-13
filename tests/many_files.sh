#!/bin/bash
source `dirname $0`/checks.sh
start_test
getcpus

#$iterations="$(( 14400 / $cpucount ))"
iterations=2048

# if we're running 'locally' then use a ramdisk (in /dev/shm)
outdir=.
if [[ "$MODE" == *local* ]]; then
    startdir=`pwd`
    outdir=/dev/shm/$testname
    mkdir $outdir
    cd $outdir
fi

function do_op {
    local id=$1
    local i=$2
    local fullname=`printf "p%.5d.%d" $id $i`
    echo "hello world" > $fullname
    if [ $i -gt 0 ]; then
        last="$(( i - 1 ))"
        lastname=`printf "p%.5d.%d" $id $last`
        rm $lastname
    fi
}

function child {
    local id=$1
    local i=0

    for (( i = 0; i < $iterations; i++ )); do
        do_op $id $i
    done

    # remove the last file created
    last="$(( i - 1 ))"
    lastname=`printf "p%.5d.%d" $id $last`
    rm $lastname
}

start_ns="$(date +%s%N)"
# spawn the children and save the pids
for (( i = 0; i < $cpucount; i++ )); do
    child $i &
    children[$i]=$!
done

# now wait for the child processes to finish
for (( i = 0; i < $cpucount; i++ )); do
    wait ${children[$i]}
done

# calculate the time spent
elapsed="$(($(date +%s%N)-start_ns))"
sec="$((elapsed/1000000000))"
us="$((elapsed/1000))"
ms="$((elapsed/1000000))"
subms="$((ms - (sec * 1000)))"

printf "$testname time to test: %d.%.3d sec\n" $sec $subms

if [[ "$MODE" == *local* ]]; then
    cd $startdir
    remove_dir $outdir
fi

end_test
