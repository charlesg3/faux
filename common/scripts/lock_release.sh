#!/bin/bash
./common/sys/mount_cag.sh
if [ $? -ne 0 ]; then exit -1; fi
/data/cag/u/beckmann/lock/release.sh $@
