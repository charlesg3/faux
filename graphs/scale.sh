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
    datafile=$GRAPHS/scale.data
    echo -en "c " > $datafile
    for testname in ${tests[@]} ; do
        echo -en "\"${testname/_/ }\" " >> $datafile
    done
    echo "" >> $datafile

	declare -A base_times
    for cores in "${corecounts[@]}" ; do
        echo -n $cores "  " >> $datafile
        for testname in ${tests[@]} ; do
            if [ "$cores" == "1" -a "$testname" == "mailbench" ]; then
                cores=2
            fi
            if [ "$testname" == "build_linux" ]; then
                tot_time=`cat $GRAPHS/$testname.hare_poll.$cores | grep "time to" | awk '{ print $5 }'`
            else
                tot_time=`cat $GRAPHS/$testname.hare.$cores | grep "time to" | awk '{ print $5 }'`
            fi

            if [ "$cores" == "2" -a "$testname" == "mailbench" ]; then
                   cores=1
            fi

#            if [[ "build_linux|punzip" =~ "$testname" ]]; then
#                tot_time=`float_eval 1 / $tot_time`
#            fi

            if [ "$cores" == "1" ]; then
                    base_times[$testname]=$tot_time
            fi

            if [[ "build_linux|punzip" =~ "$testname" ]]; then
                tot_time=`float_eval ${base_times["$testname"]} / $tot_time`
            else
                tot_time=`float_eval $cores \* ${base_times["$testname"]} / $tot_time`
            fi
            echo -n $tot_time "  " >> $datafile
        done
        echo "" >> $datafile
    done
}

corecounts=( `get_conf $graph_src build_linux "corecounts"` )
collect_data
