#!/bin/bash

shopt -s expand_aliases
alias e='CURLINE=$0:$LINENO'
alias dir_exist='e _dir_exist'
alias dir_nexist='e _dir_nexist'
alias file_exist='e _file_exist'
alias file_nexist='e _file_nexist'
alias link_exist='e _link_exist'
alias files_same='e _files_same'
alias files_diff='e _files_diff'
alias remove_file='e _remove_file'
alias remove_dir='e _remove_dir'
alias make_enter='e _make_enter'
alias leave_rm='e _leave_rm'
alias make_file='e _make_file'
alias make_dir='e _make_dir'
alias start_test='e _start_test'
alias end_test='e _end_test'
alias matches='e _matches'

function getbase {
filebase=${1##*/} # remove everything up to the last /
filebase=${filebase%%.*} # remove everything after last .
echo $filebase
}

testname=`getbase $0`

function getcpus {
if [ -n "$THREADS" ]; then
cpucount=$THREADS
else
cpucount=$[`cat /proc/cpuinfo | grep -e "^$" | wc -l` - 1]
fi
export cpucount
}

function fail {
printf "%-52s" "Test: $testname"
echo -e "[ \e[00;31mFAIL\e[00m ]"
exit -1
}

function success {
printf "%-52s" "Test: $testname"
echo -e "[  \e[00;32mOK\e[00m  ]"
exit 0
}

function _matches {
if [ "$1" != "`echo -en \"$2\"`" ]
then
    echo "$CURLINE: output doesn't match: \"$1\" vs \"`echo -en $2`\""
    fail
fi
}

function _dir_exist {
if [ ! -d $1 ]
then
    echo "$CURLINE: dir dne: " $1
    fail
fi
}

function _dir_nexist {
if [ -d $1 ]
then
    echo "$CURLINE: dir exists: " $1
    fail
fi
}

function _file_exist {
if [ ! -f $1 ]
then
    echo "$CURLINE: file dne: " $1
    fail
fi
}

function _file_nexist {
if [ -f $1 ]
then
    echo "$CURLINE: file exists: " $1
    fail
fi
}

function _link_exist {
if [ ! -h $1 ]
then
    echo "$CURLINE: link dne: " $1
    fail
fi
}

function _files_same {
_file_exist $1
_file_exist $2
if ! diff $1 $2 >/dev/null
then
    echo "$CURLINE: files are different: $1 and $2"
    fail
fi
}

function _files_diff {
if diff $1 $2 >/dev/null
then
    echo "$CURLINE: files are same: $1 and $2"
    fail
fi
}

function _make_file {
echo $2 > $1
_file_exist $1
}

function _remove_file {
rm $1
_file_nexist $1
}

function _make_dir {
mkdir -p $1
_dir_exist $1
}

function _remove_dir {
rm -r $1
_dir_nexist $1
}

function _make_enter {
mkdir $1
_dir_exist $1
cd $1
}

function _leave_rm {
cd ..
rmdir $1 || fail
_dir_nexist $1
}

function _start_test {
_make_enter $testname
}

function _end_test {
_leave_rm $testname
success
}

# arg0: file to download
# arg1: where to get it from
# directory it extrats to defaults to file without extension
# defaults for download_extract4...
function download_extract {
dest=${1##*/} # remove everything up to the last /
dest=${dest%%.*} # remove everything after last .
download_extract4 $dest $1 $2 $HARE_CONTRIB
}

function download_extract3 {
download_extract4 $1 $2 $3 $HARE_CONTRIB
}

# arg1: file to download
# arg2: directory it will extract to
# arg3: source to downoad from (full path - file)
# arg4: direcotry where downloaded file will live
function download_extract4 {
if [ ! -e $4/$1 ]; then
	echo "Downloading $file..."
	wget -P $4 $3/$1
fi

if [ ! -d $2 ]; then
    echo "Extracting $2"
    local start_ns="$(date +%s%N)"
    tar -xf $4/$1
    local elapsed_ns="$(($(date +%s%N)-start_ns))"
    local sec="$((elapsed_ns/1000000000))"
    local ms="$((elapsed_ns/1000000))"
    local subms="$((ms - (sec * 1000)))"
    local ms="$((elapsed_ns/1000000))"
    printf "Extract took: %d.%.3d sec\n" $sec $subms
fi
}

