#!/bin/bash

sentinel=/data/cag/projects

# force autofs to mount if its running
ls ${sentinel} >/dev/null 2>&1

if [ ! -d ${sentinel} ]
then
    sudo mkdir -p /data/cag
    sudo mount -o rsize=32768,wsize=32768,tcp phobos.csail.mit.edu:/cag /data/cag
fi
