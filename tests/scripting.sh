#!/bin/bash
source `dirname $0`/checks.sh
start_test

python $HARE_ROOT/tests/pycf.py
file_exist f
matches "`cat f`" "hithere"
perl -e "open(F, '>>f2'); close (F);"
file_exist f2

remove_file f
remove_file f2

# test parent directories

# move deeper in path heirarchy
mkdir -p d1/d2/d3/d4
cd d1/d2/d3/d4
path=`python << EOF
import os
print os.getcwd()
EOF`
matches "`pwd`" $path

# move back up in path heirarchy
cd ../../../../
path=`python << EOF
import os
print os.getcwd()
EOF`
matches "`pwd`" $path
rm -r d1

# test non-interactive mode
python << EOF
f = open('f','w')
f.write("Hello, world!\\n")
f.close()
f2 = open('f','r')
if(f2.read() != "Hello, world!\\n"):
	print "ERROR: couldn't read what we wrote."
f2.close()
EOF

matches "`cat f`" "Hello, world!\\n"
remove_file f

cat - > sub.sh << EOF
exec /bin/true 2>/dev/null
EOF

if [ ! $? -eq 0 ]
then
    fail
fi

cat - > sub.sh << EOF
exec /bin/bash -c "/bin/true 2>/dev/null"
EOF

bash sub.sh || fail

rm sub.sh

dash -c "{ echo -n | cat ; } ; exec 7<&0 "
if [ ! $? -eq 0 ]; then fail; fi

cat - > sub.sh << EOF
#! /bin/sh
exec 5>>out
{
cat <<_EOF
## --------- ##
_EOF
} >&5
EOF

chmod +x sub.sh
./sub.sh || fail

rm out
rm sub.sh

touch f
dash -c "if ! cat < /dev/null ; then false ; fi < f" || fail
remove_file f

echo "asdfghjkl" > in
matches `/bin/sh -c "if true < /dev/null ; then cat ; fi < in"` "asdfghjkl"
rm in

end_test
