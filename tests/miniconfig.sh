#!/bin/bash
source `dirname $0`/checks.sh
start_test

mkdir -p /tmp

exec 7<&0 </dev/null
exec 6>&1

i=0
while [ $i -lt 64 ] ; do

    if cat - </dev/null 2>/dev/null; then
        true
    else
        true
    fi

    touch ./tempfile
    if cat - < /dev/null > /dev/null 2>&1; then
        cat
    else
        cat
    fi < ./tempfile > ./tempfile2
    rm tempfile tempfile2

    eval sed \"\" "" | if true; then cat - ; fi >./out
    rm -f "out2" && mv "./out" "out2"

    eval sed \"\" "" | if true; then cat - ; fi >./out
    rm -f out2 && mv ./out out2
    rm -f out2

    i=$[ i + 1 ]

done

rm -r /tmp

end_test

