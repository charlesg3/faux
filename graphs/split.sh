#!/bin/bash

declare -a tests=( creates writes renames directories pfind_dense pfind_sparse punzip mailbench fsstress build_linux )

PROC_COUNT=40
HARE_BUILD=/home/cg3/hare/build
GRAPHS=$HARE_BUILD/graphs

float_scale=4

#####################################################################
# Evaluate a floating point number expression.

function float_eval()
{
    local stat=0
    local result=0.0
    if [[ $# -gt 0 ]]; then
        result=$(echo "scale=$float_scale; $*" | bc -q 2>/dev/null)
        stat=$?
        if [[ $stat -eq 0  &&  -z "$result" ]]; then stat=1; fi
    fi
    echo $result
    return $stat
}


#####################################################################
# Evaluate a floating point number conditional expression.

function float_cond()
{
    local cond=0
    if [[ $# -gt 0 ]]; then
        cond=$(echo "$*" | bc -q 2>/dev/null)
        if [[ -z "$cond" ]]; then cond=0; fi
        if [[ "$cond" != 0  &&  "$cond" != 1 ]]; then cond=0; fi
    fi
    local stat=$((cond == 0))
    return $stat
}


function hare_set_config()
{
    local key=$1
    local value=$2
    ./common/scripts/setconfig.sh $key $value
}

function default_conf()
{
    local fs_server_count=$1
    hare_set_config SERVER_COUNT 40
    hare_set_config FS_SERVER_COUNT $fs_server_count
    hare_set_config SCHEDULER_RR y
    hare_set_config SCHEDULING_RAND n
    hare_set_config REMOTE_EXEC y

    hare_set_config SCHEDULING_RAND n
    hare_set_config SCHEDULING_NONFS n
    hare_set_config SCHEDULING_NONFS_SEQ y
    hare_set_config SCHEDULING_SEQUENTIAL n
}

function mailbench_conf()
{
    rm /var/run/shm/notify
}

function punzip_conf()
{
    hare_set_config SCHEDULING_NONFS_SEQ n
    hare_set_config SCHEDULING_NONFS y
    return
#    hare_set_config REMOTE_EXEC n
}

function build_linux_conf()
{
    hare_set_config SCHEDULING_NONFS_SEQ n
    hare_set_config SCHEDULING_NONFS y
}

function setup()
{
    local app=$1
    local fs_server_count=$2
    default_conf $fs_server_count
    if [ "$app" == "mailbench" ]; then
        mailbench_conf
    elif [ "$app" == "build_linux" ] ; then
        build_linux_conf
    elif [ "$app" == "punzip" ] ; then
        punzip_conf
    fi
    make cleanup hare -j 42 2>&1 > /dev/null
}

#triangle
cc=( 1s.39 2s.38 4s.36 6s.34 8s.32 10s.30 12s.28 14s.26 16s.24 18s.22 20s.20 )

function gen_data()
{
    SETTING=$1
    hare_set_config DIRECTORY_DISTRIBUTION y
    hare_set_config BUFFER_CACHE y
    hare_set_config DIRECTORY_CACHE y
    hare_set_config RM_SCATTER_GATHER y
    hare_set_config DENT_SNAPSHOT y
    hare_set_config CREATION_AFFINITY y

    for t in ${tests[@]} ; do
        for c in ${cc[@]} ; do
            servers=${c%%s.*}
            apps=${c##*s.}
            dpf=$GRAPHS/$t.$c
            if [ ! -e $dpf ]; then
                echo "generating: $dpf"
                setup $t $servers
                make $t THREADS=$apps > $dpf
            fi
        done
    done
}

function collect_data()
{
    printf "test     \tservers\tbest\ttwenty\tshared\n"
    for t in ${tests[@]} ; do
        local best=0.0
        local best_servers=0
        for c in ${cc[@]} ; do
            servers=${c%%s.*}
            apps=${c##*s.}
            dpf=$GRAPHS/$t.$c
            if [ ! -e $dpf ]; then
                echo "$s not found!" $dpf
                exit -1
            fi
            tot_time=`cat $dpf | grep "time to" | awk '{ print $5 }'`

            if [[ "build_linux|punzip" =~ "$t" ]]; then
                throughput=`float_eval 1 / $tot_time`
            else
                throughput=`float_eval ${apps} / $tot_time`
            fi

            if float_cond "$throughput > $best" ; then
                best=$throughput
                best_servers=$servers
            fi
        done

        tot_time_20=`cat $GRAPHS/$t.20s.20 | grep "time to" | awk '{ print $5 }'`
        if [[ "build_linux" =~ "$t" ]]; then
            tot_time_shared=`cat $GRAPHS/$t.hare_poll.40 | grep "time to" | awk '{ print $5 }'`
        else
            tot_time_shared=`cat $GRAPHS/$t.hare.40 | grep "time to" | awk '{ print $5 }'`
        fi

        if [[ "build_linux|punzip" =~ "$t" ]]; then
            throughput_shared=`float_eval 1 / $tot_time_shared`
            throughput_20=`float_eval 1 / $tot_time_20`
        else
            throughput_shared=`float_eval 40 / $tot_time_shared`
            throughput_20=`float_eval 20 / $tot_time_20`
        fi

        printf "%s\t%d\t%0.4f\t%0.4f\t%0.4f\n" "\"${t/_/ }\"" $best_servers $best $throughput_20 $throughput_shared
    done

}

gen_data
collect_data
