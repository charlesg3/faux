#!/bin/bash
graph_src=`dirname $0`

function get_conf()
{
    local base=$1
    local name=$2
    local key=$3
    cat $base/conf/$name.conf | grep $key | sed "s/$key[ ]*//" | sed 's/"//g'
}

function hare_set_config()
{
    local key=$1
    local value=$2
    $graph_src/../../common/scripts/setconfig.sh $key $value
}

function run_linux()
{
    local testname=$1
    local cores=$2
    ps ax | grep faux.py | grep -v grep | wc -l > /dev/null
    make cleanup $testname THREADS=$cores MODE=local
}

function run_test()
{
    local testname=$1
    local cores=$2
    make $testname THREADS=$2
}

function hare_run_clean()
{
    local testname=$1
    local cores=$2
    make cleanup hare -j 42
    make $testname THREADS=$cores
}

function run_hare_poll()
{
    local testname=$1
    local cores=$2
    hare_set_config SERVER_COUNT $cores
    hare_set_config FS_SERVER_COUNT $cores
    hare_set_config SCHEDULER_RR n
    hare_set_config SCHEDULING_RAND y
    hare_set_config DIRECTORY_DISTRIBUTION y
    hare_run_clean $1 $cores
}

function run_hare()
{
    local testname=$1
    local cores=$2
    hare_set_config SERVER_COUNT $cores
    hare_set_config FS_SERVER_COUNT $cores
    hare_set_config CREATION_AFFINITY y
    hare_set_config SCHEDULER_RR y
    hare_set_config SCHEDULING_RAND n
    hare_set_config DIRECTORY_DISTRIBUTION y
    hare_run_clean $1 $cores
}

function run_hare_nodd()
{
    local testname=$1
    local cores=$2
    hare_set_config SERVER_COUNT $cores
    hare_set_config FS_SERVER_COUNT $cores
    hare_set_config CREATION_AFFINITY y
    hare_set_config SCHEDULER_RR y
    hare_set_config SCHEDULING_RAND n
    hare_set_config DIRECTORY_DISTRIBUTION n
    hare_run_clean $1 $cores
}

