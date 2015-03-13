#!/bin/bash

declare -a tests=( creates writes renames directories rm_dense rm_sparse pfind_dense pfind_sparse extract punzip mailbench fsstress build_linux )
PROC_COUNT=1
HARE_BUILD=/home/cg3/hare/build/
GRAPHS=$HARE_BUILD/graphs

function hare_set_config()
{
    local key=$1
    local value=$2
    ./common/scripts/setconfig.sh $key $value
}

function default_conf()
{
    hare_set_config SERVER_COUNT $PROC_COUNT
    hare_set_config FS_SERVER_COUNT $PROC_COUNT
    hare_set_config SCHEDULER_RR y
    hare_set_config SCHEDULING_RAND n
    hare_set_config SCHEDULING_NONE n
    hare_set_config SCHEDULING_STRIPE n
    hare_set_config SCHEDULING_NON_SERVER n
    hare_set_config SCHEDULING_CORE0 n
    hare_set_config SCHEDULING_SOCKET0 n
    hare_set_config SCHEDULING_SOCKET1 n
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

function build_linux_conf()
{
    hare_set_config SCHEDULER_RR n
}

function setup()
{
    local app=$1
    default_conf
    if [ "$app" == "mailbench" ]; then
        mailbench_conf
    elif [ "$app" == "build_linux" ] ; then
        build_linux_conf
    fi
}

function gen_data()
{
    echo "Core 0"
    for t in ${tests[@]} ; do
        dpf=./build/graphs/$t.CORE0
        if [ ! -e $dpf ]; then
            THREADS=1
            if [ "$t" == "mailbench" ]; then
                THREADS=2
            fi
            echo "running: $t"
            setup $t
            hare_set_config SCHEDULING_CORE0 y
            hare_set_config REMOTE_EXEC n
            hare_set_config PCIDE y
            make cleanup hare -j 42 2>&1 > /dev/null
            make $t THREADS=$THREADS > $dpf
        fi
    done

    echo "Socket 0"
    for t in ${tests[@]} ; do
        dpf=./build/graphs/$t.SOCKET0
        if [ ! -e $dpf ]; then
            THREADS=1
            if [ "$t" == "mailbench" ]; then
                THREADS=2
            fi
            echo "running: $t"
            setup $t
            hare_set_config SCHEDULING_SOCKET0 y
            hare_set_config REMOTE_EXEC n
            hare_set_config SCHEDULER_RR n
            make cleanup hare -j 42 2>&1 > /dev/null
            make $t THREADS=$THREADS > $dpf
        fi
    done

    echo "Socket 1"
    for t in ${tests[@]} ; do
        dpf=./build/graphs/$t.SOCKET1
        if [ ! -e $dpf ]; then
            THREADS=1
            if [ "$t" == "mailbench" ]; then
                THREADS=2
            fi
            echo "running: $t"
            setup $t
            hare_set_config SCHEDULING_SOCKET1 y
            hare_set_config REMOTE_EXEC n
            hare_set_config SCHEDULER_RR n
            make cleanup hare -j 42 2>&1 > /dev/null
            make $t THREADS=$THREADS > $dpf
        fi
    done

    make cleanup
    echo "Linux"
    for t in ${tests[@]} ; do
        dpf=./build/graphs/$t.linux.0
        if [ ! -e $dpf ]; then
            THREADS=1
            if [ "$t" == "mailbench" ]; then
                THREADS=2
            fi
            echo "running: $t"
            setup $t
            make $t THREADS=$THREADS MODE=local > $dpf
        fi
    done
}

function collect_data()
{
    datafile=$GRAPHS/overhead.data
    echo -e "test hare \"hare socket0\" \"hare socket1\" linux" > $datafile

    for testname in ${tests[@]} ; do
        echo -en "\"${testname/_/ }\" " >> $datafile
        tot_time=`cat $GRAPHS/$testname.CORE0 | grep "time to" | awk '{ print $5 }'`
        echo -n $tot_time "  " >> $datafile
        tot_time=`cat $GRAPHS/$testname.SOCKET0 | grep "time to" | awk '{ print $5 }'`
        echo -n $tot_time "  " >> $datafile
        tot_time=`cat $GRAPHS/$testname.SOCKET1 | grep "time to" | awk '{ print $5 }'`
        echo -n $tot_time "  " >> $datafile
        tot_time=`cat $GRAPHS/$testname.linux.0 | grep "time to" | awk '{ print $5 }'`
        echo -n $tot_time "  " >> $datafile
        echo "" >> $datafile
    done
}

gen_data
collect_data
