#!/bin/bash
if [ "$2" == "n" ]; then
    awk -W exec - .config > .config~ << EOF
/CONFIG_$1=/  || /CONFIG_$1 / {
  print "# CONFIG_$1 is not set";
}
! /CONFIG_$1=/ && ! /CONFIG_$1 /{
 print \$0
}
EOF
else
    awk -W exec - .config > .config~ << EOF
/CONFIG_$1=/  || /CONFIG_$1 / {
  print "CONFIG_$1" "=" "$2";
}
! /CONFIG_$1=/ && ! /CONFIG_$1 /{
 print \$0
}
EOF
fi

diff .config~ .config > /dev/null 2>&1
if [ $? -eq 0 ]; then
    true
elif [ $? -eq 1 ]; then
    cp .config~ .config
else
    echo "There was something wrong with the diff command"
fi
