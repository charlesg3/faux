#!/bin/bash

function parse_stats()
{
    local outfile=$1
    local datafile=$2
    awk -W exec - $outfile > $datafile << EOF
BEGIN {
  foundops=0;
}
/----------------------------/ {
  foundops=0;
}
{
  if ( foundops != 0 ) {
    foundops++;
    if ( foundops == 2 ) {
      printf("# ");
    }
    if ( \$2 == 0 ) {
      next;
    }
    print \$0
  }
}
/Ops/ {
  foundops=1;
}
EOF
}

function stats_app()
{
    local app=$1
    base=$results/$app.ops
    outfile=$base
    datafile=$base.data

    if [ ! -e $outfile ]; then
        echo "Generating stats: " $outfile
        make cleanup $app stats > $outfile
    fi

    if [ ! -e $datafile ]; then
        parse_stats $outfile $datafile
    fi

    # sort the file
    cat $datafile | grep "#" > $datafile.header
    cat $datafile.header && cat $datafile | grep -v "^#" | awk '{ print $3, $1, $2, $4 }' | sort -rg | awk '{ printf("%10s %10s %10s %10s\n",$2,$3,$1,$4) }' > $datafile.sorted
    mv $datafile.sorted $datafile
    rm $datafile.header
}

results=$1
app=$2
stats_app $app

