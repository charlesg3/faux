#!/bin/bash
source `dirname $0`/graphlib.sh

function gen_plotheader()
{
    local opsdatafile=$1
    local datafile=$2
    local app=$3
    local output=$4
    local title=$5
    local subtitle=$6
    local xaxis=$7
    local yaxis=$8
    cat - << EOF
cyan = "#99ffff"; blue = "#4671d5"; red = "#ff0000"; orange = "#f36e00"
set decimal locale
set auto y
#set style data histogram
#set style histogram cluster gap 1
#set style fill solid border -1
set boxwidth 0.9
set xtic scale 0

set terminal svg enhanced size 600,400
set output "$output"

set title "{/=20 $title}"  font ",20" offset character 0, 0.5, 0
set tmargin at screen 0.8
set bmargin at screen 0.15
set key samplen 1.5 spacing 1 height .5 font ",12" width 0 maxcols 8
set key vert Right box
set key at screen 0,1 left top
set xlabel "$xaxis" offset character 0, 0.5, 0
set xrange [0:1025]
set xtics (64,128,256,512,1024)
set ytics nomirror
set ylabel "$yaxis"
set format y "%.1s %c"
set grid y
set style data linespoints

plot "$datafile" \\
EOF
}

function gen_plotfooter()
{
cat - << EOF
# unset multiplot
EOF
}

function gen_datasets()
{
    colors=(red blue orange cyan purple green "#0085a2" "#636363" black)
    local datasets=( ${!1} ) # read the full array first parameter
    typeset -i i len
    len=${#datasets[@]}
    for ((i=0;i<len;++i)); do
        let col=i+2
        if [[ i -eq 0 ]] ; then
            echo -n "    "
        else
            echo -n ", ''"
        fi
        echo -n "u 1:$col with linesp lt 1 lw 2 pt 7 lc rgb '${colors[i]}' ti '  ${datasets[i]/_/ }'"


        if [[ i+1 -eq len ]] ; then
            echo "  "
        else
            echo "\\"
        fi
    done
}

dir=$1
app=$2
base=$dir/$app
subtitle=`get_conf $graph_src $app "desc"`
title=`get_conf $graph_src $app "title"`
xaxis=`get_conf $graph_src $app "xaxis"`
yaxis=`get_conf $graph_src $app "yaxis"`
declare -a datasets=( `get_conf $graph_src $app "datasets"` )
plotfile=$base.plot

gen_plotheader $base.ops.data $base.data ${app/_/-} $base.svg "$title" "$subtitle" "$xaxis" "$yaxis" > $plotfile
gen_datasets datasets[@] >> $plotfile
gen_plotfooter >> $plotfile

