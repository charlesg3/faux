#!/bin/bash
source `dirname $0`/graphlib.sh

function time_app()
{
    local app=$1
    datafile=$results/$app.data
    declare -a datasets=( `get_conf $graph_src $app "datasets"` )
    rm -f $datafile
    touch $datafile

    echo -en "#results for: $app\n# c " >> $datafile
    for config in "${datasets[@]}"; do
        echo -n $config "  " >> $datafile
    done
    echo "" >> $datafile

    # run the test
    for cores in "${corecounts[@]}"
    do
        for dataset in "${datasets[@]}"
        do
            outfile=$results/$app.$dataset.$cores
            # if [ "$app" == "mailbench" ]; then
            #     cores2x="$(( cores * 2 ))"
            #     outfile=$results/$app.$dataset.${cores2x}
            # fi
            if [ ! -e $outfile ]; then
                echo $outfile
                runscript=$graph_src/conf/$app.sh
                if [ ! -e $runscript ]; then
                    runscript=$graph_src/conf/run.sh
                fi
                $runscript $app $dataset $cores $outfile
                if [ ! -e $outfile ]; then
                    echo "Error -- datafile not found: " $outfile
                    rm $datafile
                    exit -1
                fi
            fi
        done
    done

    # aggregate the results
    for cores in "${corecounts[@]}"
    do
        echo -n $cores "  " >> $datafile
        for dataset in "${datasets[@]}"
        do
            outfile=$results/$app.$dataset.$cores
            #if [ "$app" == "mailbench" ]; then
            #    cores2x="$(( cores * 2 ))"
            #    outfile=$results/$app.$dataset.${cores2x}
            #fi
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
corecounts=( `get_conf $graph_src $app "corecounts"` )

time_app $app

