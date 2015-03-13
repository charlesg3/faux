#!/bin/bash
source `dirname $0`/checks.sh
start_test

if [[ "$MODE" == *local* ]]; then
	startdir=`pwd`
	outdir=/run/shm/$testname
	mkdir $outdir
	cd $outdir
fi

# copy over the manpages
make_dir man
cd man
cp -r /usr/share/man/* .

# remove symlinks
find . -type l -exec rm {} \;

# unzip
getcpus


cd ..

cp -r man man2
cp -r man man3

find . -name "*.gz" | xargs -n 200 -P 40 gunzip

# create file listing
#find `pwd` -type f -exec stat -c "%s %n" {} \; | sort -rg | awk '{print $2}' > files
find `pwd` -type f > files

# create temp dirs for dbs
dbdir=./db$$
make_dir $dbdir
make_dir $dbdir/db
for (( i=0; i<$cpucount; i++)); do
  make_dir $dbdir/db/db$i
done

cp $HARE_CONTRIB/psearchy/mkdb.config .

start_ns=`date +%s%N`

# create the db
$HARE_CONTRIB/psearchy/mkdb/pedsort -p -t $dbdir/db/db -c $cpucount -m 32 < files

query_start_ns=`date +%s%N`

# perform a search
$HARE_CONTRIB/psearchy/query/qe -t $dbdir/db/db -c $cpucount -l -q "the" > /dev/null

end_ns=`date +%s%N`

elapsed_ns=$[end_ns-start_ns]
mkdb_ns=$[query_start_ns-start_ns]
query_ns=$[end_ns-query_start_ns]

sec=$[elapsed_ns/1000000000]
subms=$[elapsed_ns/1000000 - (sec * 1000)]

mkdb_sec=$[mkdb_ns/1000000000]
mkdb_subms=$[mkdb_ns/1000000 - (mkdb_sec * 1000)]

query_sec=$[query_ns/1000000000]
query_subms=$[query_ns/1000000 - (query_sec * 1000)]

echo
printf "Psearchy time to mkdb+query: %d.%.3d sec\n" $sec $subms
printf "Psearchy mkdb took: %d.%.3d sec\n" $mkdb_sec $mkdb_subms
printf "Psearchy query took: %d.%.3d sec\n" $query_sec $query_subms

remove_file mkdb.config
remove_file files
remove_dir $dbdir
remove_dir man
remove_dir man2
remove_dir man3

if [[ "$MODE" == *local* ]]; then
	cd $startdir
	remove_dir $outdir
fi

end_test
