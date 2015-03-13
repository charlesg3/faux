#!/bin/bash
source `dirname $0`/checks.sh
start_test
getcpus

# params
iterations=100
procs=$((cpucount - 1))
start_ns="$(date +%s%N)"

function do_op {
    fullname=`printf "f%.5d" $1`
    echo "Hello, world!" | cat - > $fullname
    bin=$(( $RANDOM % 12 ))
    count=$RANDOM
    for (( i = 0; i < $bin; i++ ))
    do
        (( count += 32767 ))
    done
    truncate -s $count $fullname
    dash -c "cp $fullname $fullname.bak"
    rm $fullname $fullname.bak
}

for (( i = 0; i < $iterations; i++ ))
do
    # spawn the proc and save the pid
    do_op $i &
    children[$i]=$!

    # if we've spawned as many as we want concurrently
    # then wait for the first one we spawned to finish
    if (( i > $procs )); then
        wait ${children[$(( i - procs ))]}
    fi
done

# now wait for the remainder to finish
for (( i = $iterations - $procs; i < $iterations; i++ ))
do
    wait ${children[$i]}
done

# calculate the time spent
elapsed="$(($(date +%s%N)-start_ns))"
sec="$((elapsed/1000000000))"
us="$((elapsed/1000))"
ms="$((elapsed/1000000))"
subms="$((ms - (sec * 1000)))"

per_iteration=$(( us / $iterations ))
#per_iteration=`printf "scale=2; %d / %d\n" $ms $iterations | bc`
printf "$testname took: %d.%.3d sec (%s us per iteration)\n" $sec $subms $per_iteration

end_test
