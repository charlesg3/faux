#!/bin/bash

declare -a tests=( creates writes renames directories rm_dense rm_sparse pfind_dense pfind_sparse extract punzip mailbench fsstress build_linux )

PROC_COUNT=40
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
    hare_set_config REMOTE_EXEC y
}

function mailbench_conf()
{
    rm /var/run/shm/notify
}

function punzip_conf()
{
    hare_set_config REMOTE_EXEC n
}

function build_linux_conf()
{
    hare_set_config SCHEDULER_RR n
    hare_set_config SCHEDULING_RAND y
}

function setup()
{
    local app=$1
    default_conf
    if [ "$app" == "mailbench" ]; then
        mailbench_conf
    elif [ "$app" == "build_linux" ] ; then
        build_linux_conf
    elif [ "$app" == "punzip" ] ; then
        punzip_conf
    elif [ "$app" == "extract" ] ; then
        build_linux_conf
    fi
    make cleanup hare -j 42 2>&1 > /dev/null
}

function gen_data()
{
    SETTING=$1
    hare_set_config DIRECTORY_DISTRIBUTION y
    hare_set_config BUFFER_CACHE y
    hare_set_config DIRECTORY_CACHE y
    hare_set_config RM_SCATTER_GATHER y
    hare_set_config DENT_SNAPSHOT y
    hare_set_config CREATION_AFFINITY y

    echo "turning on $SETTING"
    if [ "$SETTING" == "SCATTER_GATHER" ]; then
        hare_set_config RM_SCATTER_GATHER y
        hare_set_config DENT_SNAPSHOT y
    else
        hare_set_config $SETTING y
    fi

    for t in ${tests[@]} ; do
        if [ "$SETTING" == "CREATION_AFFINITY" -a "$t" == "build_linux" ]; then
            continue
        fi

        dpf=$GRAPHS/$t.$SETTING.y
        if [ ! -e $dpf ]; then
            echo "running: $t"
            setup $t
            make $t THREADS=$PROC_COUNT > $dpf
        fi
    done

    echo "turning off $SETTING"
    if [ "$SETTING" == "SCATTER_GATHER" ]; then
        hare_set_config RM_SCATTER_GATHER n
        hare_set_config DENT_SNAPSHOT n
    else
        hare_set_config $SETTING n
    fi

    rm /var/run/shm/notify
    for t in ${tests[@]} ; do
        if [ "$SETTING" == "CREATION_AFFINITY" -a "$t" == "build_linux" ]; then
            continue
        fi

        dpf=$GRAPHS/$t.$SETTING.n
        if [ ! -e $dpf ]; then
            echo "running: $t"
            setup $t
            make $t THREADS=$PROC_COUNT > $dpf
        fi
    done
}

function collect_data()
{
    SETTING=$1
    if [ "$SETTING" == "BUFFER_CACHE" ]; then
        of=bc
    elif [ "$SETTING" == "SCATTER_GATHER" ]; then
        of=sg
    elif [ "$SETTING" == "DIRECTORY_CACHE" ]; then
        of=dc
    elif [ "$SETTING" == "DIRECTORY_DISTRIBUTION" ]; then
        of=dd
    elif [ "$SETTING" == "CREATION_AFFINITY" ]; then
        of=ca
    else
        echo "Error, unknown setting: $SETTING."
        exit -1
    fi

    datafile=$GRAPHS/${of}.data
    
    echo -e "# $SETTING n y\ndummy 0 0" > $datafile

    for testname in ${tests[@]} ; do
#        if [ "$SETTING" == "CREATION_AFFINITY" -a "$t" == "build_linux" ]; then
#            continue
#        fi
        echo -en "\"${testname/_/ }\" " >> $datafile
        tot_time=`cat $GRAPHS/$testname.$SETTING.n | grep "time to" | awk '{ print $5 }'`
        echo -n $tot_time "  " >> $datafile
        tot_time=`cat $GRAPHS/$testname.$SETTING.y | grep "time to" | awk '{ print $5 }'`
        echo -n $tot_time "  " >> $datafile
        echo "" >> $datafile
    done
}

gen_data $1
collect_data $1


