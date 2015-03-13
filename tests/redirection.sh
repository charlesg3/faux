#!/bin/bash
source `dirname $0`/checks.sh
start_test

bash -c "echo 'hi' > hi"
file_exist hi
echo "hello world." | cat - > helloworld
file_exist helloworld
echo "hello world." > helloworld2
file_exist helloworld2
files_same helloworld helloworld2

remove_file hi
remove_file helloworld
remove_file helloworld2

# this somehow created negative file-descriptor counts
# so it must exercise some duping code
touch bare1
if sed "" ; then sed "" ; sed "" ; sed "" ; fi < bare1 > bare2
remove_file bare1
remove_file bare2

# this test was not closing all fds
readlink notfound > link
remove_file link

echo -n "start" | (cat -; echo -n "end" ) > of
matches "`cat of`" "startend"
remove_file of

exec 5>>tmp
echo "" | cat - >&5
file_exist tmp
remove_file tmp

exec 4>>tmp
echo "" | cat >&4
file_exist tmp
remove_file tmp

# this created an orphan at one point as dash
# wasn't exiting through the normal path
exec 5>>orphan
{
  for ((i=0;i<1024;i++)); do
    echo "          ";
  done
} >&5
/bin/sh -c ""
file_exist orphan
rm orphan

# this test causes a sigpipe that the system
# must catch in order to close all open fds
# so as not to create orphans / leak fds
echo hi >f
exec 5>>f
cat f 2>&5 | true
rm f

end_test

