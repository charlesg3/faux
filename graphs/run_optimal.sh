#!/bin/bash
source `dirname $0`/graphlib.sh

#for renaming mailbenchfiles...
#mkdir build/graphs/mailbenchbak
#for f in build/graphs/mailbench.*s* ; do
#    basedir=${f%%mailbench*}
#    newdir=${basedir}mailbenchbak
#    file=${f##*mailbench.hare}
#    mid=${file%%s.*}
#    ext=${f##*s.}
#    newext="$(( ext * 2 ))"
#    mv $f "$newdir/mailbench.${mid}s.$newext"
#done
#mv build/graphs/mailbenchbak/* build/graphs

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

#triangle
function set_default_cc
{
c2=(  1s.1 )
c4=(  1s.3 2s.2 3s.1 )
c8=(  1s.7 2s.6 3s.5 4s.4  6s.2 )
c12=( 2s.10 4s.8  6s.6  8s.4  )
c16=( 2s.14 4s.12 6s.10 8s.8  10s.6  )
c20=( 2s.18 4s.16 6s.14 8s.12 10s.10 12s.8  )
c24=( 2s.22 4s.20 6s.18 8s.16 10s.14 12s.12 14s.10 )
c28=( 2s.26 4s.24 6s.22 8s.20 10s.18 12s.16 14s.14 16s.12 )
c32=( 2s.30 4s.28 6s.26 8s.24 10s.22 12s.20 14s.18 16s.16 18s.14 )
c36=( 2s.34 4s.32 6s.30 8s.28 10s.26 12s.24 14s.22 16s.20 18s.18 20s.16 )
c40=( 2s.38 4s.36 6s.34 8s.32 10s.30 12s.28 14s.26 16s.24 18s.22 20s.20 22s.18 )
}

function set_mb_cc()
{
c2=(  1s.1 )
c4=(  2s.1 )
c8=(  4s.2 )
c12=( 2s.5 4s.4  6s.3 )
c16=( 2s.7 4s.6  6s.5 8s.4  )
c20=( 2s.9 4s.8  6s.7 8s.6  10s.5 )
c24=( 2s.11 4s.10 6s.9 8s.8 10s.7 12s.6 )
c28=( 2s.13 4s.12 6s.11 8s.10 10s.9 12s.8 14s.7 )
c32=( 2s.15 4s.14 6s.13 8s.12 10s.11 12s.10 14s.9 16s.8 )
c36=( 2s.17 4s.16 6s.15 8s.14 10s.13 12s.12 14s.11 16s.10 18s.9 )
c40=( 2s.19 4s.18 6s.17 8s.16 10s.15 12s.14 14s.13 16s.12 18s.11 20s.10 )
}

function set_bl_cc()
{
c2=(  1s.1 )
c4=(  1s.3 )
c8=(  1s.7 )
c12=( 1s.11 )
c16=( 1s.15 )
c20=( 1s.19 )
c24=( 1s.23 )
c28=( 1s.27 )
c32=( 1s.31 )
c36=( 1s.35 )
c40=( 1s.39 )
}

function set_cc()
{
    local app=$1
    if [[ "$app" == "build_linux" ]] ; then
        set_bl_cc
    elif [[ "$app" == "mailbench" ]] ; then
        set_mb_cc
    else
        set_default_cc
    fi
}

set_default_cc

allc=( c2 c4 c8 c12 c16 c20 c24 c28 c32 c36 c40 )

dir="build/graphs/"

function aggregate_data1()
{
    local app=$1
    local cc=$2
    local offset=$3
    set_cc $app
    arr=( $(eval echo \${$cc[@]} ))
#        echo -en "${cc##*c}:\t"
    max="0.0"
    max_config=""
    for dp in ${arr[@]} ; do
        dpf="${dir}${app}.$dp"
        if [ ! -e $dpf ]; then
            echo $dpf " not found."
        fi
        tot_time=`cat $dpf | grep "time to" | awk '{ print $5 }'`
#            echo -en "[$tot_time]\t"
        clients=${dp##*.}
        if [[ "psearchy|punzip|build_linux" =~ "$app" ]] ; then
            throughput=`float_eval 1 / $tot_time`
        else
            throughput=`float_eval $clients / $tot_time`
        fi
#            echo -en "[$throughput]\t"
        if float_cond "$throughput > $max"; then
            max=$throughput
            max_config=$dp
        fi
    done
#    echo -n $max
#        echo $max "($max_config)"
        echo -en ${max_config%%s.*}.0${offset}
}

function aggregate_data()
{
    local app=$1
    for cc in "${allc[@]}" ; do
        aggregate_data1 $app $cc 0
        echo
    done
}


function gen_data()
{
    local app=$1
    set_cc $app
    for cc in "${allc[@]}" ; do
        arr=( $(eval echo \${$cc[@]} ))
        for dp in ${arr[@]} ; do
            dpf="${dir}${app}.$dp"
            if [ ! -e $dpf ]; then
                echo -n $dpf
                dataset=${dpf#*.}
                dataset=${dataset%.*}
                cores=${dpf##*.}
                runscript=$graph_src/conf/$app.sh
                if [ ! -e $runscript ]; then
                    runscript=$graph_src/conf/run.sh
                fi
                $runscript $app $dataset $cores $dpf
                tot_time=`cat $dpf | grep "time to" | awk '{ print $5 }'`
                echo " -> " $tot_time
            fi
        done
    done
}


tests=( mailbench fsstress psearchy punzip build_linux pfind_dense pfind_sparse writes renames creates directories )
#tests=( punzip )

for t in ${tests[@]} ; do
    gen_data $t
done

echo -en "c\t"
for t in ${tests[@]} ; do
    echo -en "\"${t/_/ }\"\t"
done

echo

for cc in "${allc[@]}" ; do
    i=0
    echo -en "${cc##*c}\t"
    for t in ${tests[@]} ; do
        aggregate_data1 $t $cc $i
        echo -en "\t"
        i=$[i+1]
    done
    echo
done

