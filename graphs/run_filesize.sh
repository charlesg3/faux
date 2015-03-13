#!/bin/bash
source `dirname $0`/graphlib.sh

function time_app()
{
    local app=$1
    datafile=$results/$app.data
    plotfile=$results/$app.plot
    epsfile=$results/$app.eps
    pngfile=$results/$app.png
    declare -a datasets=( `get_conf $graph_src $app "datasets"` )
    rm -f $datafile
    touch $datafile

    echo -en "#results for: $app\n# c " >> $datafile
    for config in "${datasets[@]}"; do
        echo -n $config "  " >> $datafile
    done
    echo "" >> $datafile

    # run the test
    for dataset in "${datasets[@]}"
    do
        for size in "${filesizes[@]}"
        do
            outfile=$results/$app.$dataset.$size
            if [ ! -e $outfile ]; then
                echo $outfile
                $graph_src/conf/$app.sh $app $dataset $size $outfile
                if [ ! -e $outfile ]; then
                    echo "Error -- datafile not found: " $outfile
                    rm $datafile
                    exit -1
                fi
            fi
        done
    done

    # aggregate the results
    for size in "${filesizes[@]}"
    do
        echo -n $size "  " >> $datafile
        for dataset in "${datasets[@]}"
        do
            outfile=$results/$app.$dataset.$size
            if [ ! -e $outfile ]; then
                echo "Error -- datafile not found: " $outfile
                exit -1
            fi
            tot_time=`cat $outfile | grep "time to" | awk '{ print $5 }'`
            echo -n $tot_time "  " >> $datafile
        done
        echo "" >> $datafile
    done
}

results=$1
app=$2
filesizes=( `get_conf $graph_src $app "filesizes"` )

time_app $app

