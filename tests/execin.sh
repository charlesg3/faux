#!/bin/bash
source `dirname $0`/checks.sh
start_test

echo "#!/bin/bash" > ./script.sh
echo "echo hello \$1" >> ./script.sh
chmod +x ./script.sh

matches "`./script.sh world`" "hello world\n"

remove_file ./script.sh

end_test
