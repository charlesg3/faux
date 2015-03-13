#!/bin/bash

function convert_data()
{
declare -a tests=( mailbench fsstress psearchy punzip build_linux pfind_dense pfind_sparse rm_dense rm_sparse writes copy renames creates directories )

remove_list="\-\-\-|Ops|Name|TOTAL|DESTROY|SYMLINK|BLOCKS|FLUSH|DUP|SYMLINK|READLINK|TRUNC|CHMOD|CHOWN|LSEEK|UTIMENS"
oplista=`cat graphs/data/fsstress.ops.data | egrep -v $remove_list | awk '{ print $1 }' | tr '\n' '\t'`
oplist=( $oplista )

echo -en "TEST\t"
echo ${oplist[@]/_/-} " TOTAL"

for t in ${tests[@]} ; do
    echo -en ${t/_/-} "\t"
    cat graphs/data/$t.ops.data | egrep -v $remove_list | awk '{ print $3 }' | tr '\n' '\t'
    cat graphs/data/$t.ops.data | egrep -v $remove_list | awk '{ print $3 }' | paste -sd+ | bc
done
}

function create_plotfile()
{
    cat - << eof
cyan = "#99ffff"; blue = "#4671d5"; red = "#ff0000"; orange = "#f36e00"
set terminal svg enhanced size 600,400
set output "/home/cg3/hare/build/graphs/ops.svg"
set decimal locale
set style data histograms
set style histogram cluster gap 1
set style histogram rowstacked
set style fill solid border rgb "black"
set key out cent right invert reverse Left font ",11"
set key samplen 0.5 spacing 0.75 width 1 height 0 
set boxwidth 0.75 absolute
#set title "{/=14 Operations}"
set yrange [0:100]
set xrange [0:*]
set ylabel "Operation %"
unset ytics
set xtics rotate by -45 offset character -2, -0.5
eof

op_count=${#oplist[@]}
index=0

echo -n "plot"

colors=(red dark-blue green yellow black blue purple pink dark-red orange cyan dark-green violet)
while [ "$index" -lt "$op_count" ]
do
    col="$(( index + 2 ))"
    totcol="$(( op_count + 2 ))"
    bench=${oplist[index]/_/-}
    echo -n " 'graphs/data/ops.data' using (100.*\$$col/\$$totcol):xticlabels(1) t \"$bench\" lc rgb '${colors[index]}'"
    ((index++))
    if [ "$index" -lt "$op_count" ] ; then
        echo ', \'
    fi

done
echo
}

convert_data > graphs/data/ops.data

sed 's/build-linux/"build linux"/' -i graphs/data/ops.data
sed 's/pfind-dense/"pfind (dense)"/' -i graphs/data/ops.data
sed 's/pfind-sparse/"pfind (sparse)"/' -i graphs/data/ops.data
sed 's/rm-dense/"rm (dense)"/' -i graphs/data/ops.data
sed 's/rm-sparse/"rm (sparse)"/' -i graphs/data/ops.data

create_plotfile > graphs/plot/ops.plot

