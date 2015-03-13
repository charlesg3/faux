#!/bin/bash
source `dirname $0`/graphlib.sh
declare -a tests=( creates writes renames directories pfind_dense pfind_sparse punzip mailbench fsstress build_linux )
HARE_BUILD=/home/cg3/hare/build/
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

function collect_data()
{
    datafile=$GRAPHS/scale_shared.data
    echo -e "test hare \"hare socket0\" \"hare socket1\" linux" > $datafile

    echo -en "c " > $datafile
    for testname in ${tests[@]} ; do
        echo -en "\"${testname/_/ }\" " >> $datafile
    done
    echo "" >> $datafile

    declare -A hare_base_times
    declare -A linux_base_times
    for testname in ${tests[@]} ; do
        cores=1
        if [ "$testname" == "mailbench" ]; then
            cores=2
        fi
        if [ "$testname" == "build_linux" ]; then
            hare_tot_time=`cat $GRAPHS/$testname.hare_poll.1 | grep "time to" | awk '{ print $5 }'`
        else
            hare_tot_time=`cat $GRAPHS/$testname.hare.$cores | grep "time to" | awk '{ print $5 }'`
        fi

        linux_tot_time=`cat $GRAPHS/$testname.linux.$cores | grep "time to" | awk '{ print $5 }'`

        if [[ "build_linux|punzip" =~ "$testname" ]]; then
            hare_tot_time=`float_eval 1 / $hare_tot_time`
            linux_tot_time=`float_eval 1 / $linux_tot_time`
        fi

        hare_base_times[$testname]=$hare_tot_time
        linux_base_times[$testname]=$linux_tot_time
    done

    for testname in ${tests[@]} ; do
        echo -n $testname "  " >> $datafile
        cores=40

        if [ "$testname" == "build_linux" ]; then
            hare_tot_time=`cat $GRAPHS/$testname.hare_poll.40 | grep "time to" | awk '{ print $5 }'`
        else
            hare_tot_time=`cat $GRAPHS/$testname.hare.$cores | grep "time to" | awk '{ print $5 }'`
        fi

        linux_tot_time=`cat $GRAPHS/$testname.linux.$cores | grep "time to" | awk '{ print $5 }'`

        if [[ "build_linux|punzip" =~ "$testname" ]]; then
            hare_tot_time=`float_eval 1 / $hare_tot_time`
            linux_tot_time=`float_eval 1 / $linux_tot_time`
        fi

        hare_tot_time=`float_eval $cores \* ${hare_base_times["$testname"]} / $hare_tot_time`
        linux_tot_time=`float_eval $cores \* ${linux_base_times["$testname"]} / $linux_tot_time`
        echo -n $hare_tot_time "  " $linux_tot_time >> $datafile
        echo "" >> $datafile
    done
}

collect_data
