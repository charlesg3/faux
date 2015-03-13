#!/bin/bash
source `dirname $0`/../graphlib.sh
source `dirname $0`/../../tests/checks.sh

app=$1
dataset=$2
cores=$3
outfile=$4

run_$dataset $app $cores > $outfile

