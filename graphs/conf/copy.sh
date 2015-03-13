#!/bin/bash
source `dirname $0`/../graphlib.sh
source `dirname $0`/../../tests/checks.sh

function hare_run_filesize()
{
    local testname=$1
    local cores=$2
    local filesize=$3
    make cleanup hare -j 39
    make $testname THREADS=$cores FILESIZE=$filesize
}

function run_filesize_hare()
{
    local testname=$1
    local cores=$2
    local filesize=$3
    hare_set_config SERVER_COUNT 1
    hare_set_config CREATION_AFFINITY y
    hare_set_config NUMA_SCHEDULE n
    hare_set_config PIN_CLIENTS n
    hare_run_filesize $testname $cores $filesize
}

function run_filesize_hare_numa()
{
    local testname=$1
    local cores=$2
    local filesize=$3
    hare_set_config SERVER_COUNT 1
    hare_set_config CREATION_AFFINITY y
    hare_set_config NUMA_SCHEDULE y
    hare_set_config PIN_CLIENTS n
    hare_run_filesize $testname $cores $filesize
}

app=$1
dataset=$2
filesize=$3
outfile=$4
cores=1

run_filesize_$dataset $app $cores $filesize > $outfile

