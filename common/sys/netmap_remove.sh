#!/bin/bash

MODULES="ixgbe e1000 e1000e"

for i in $MODULES
do
    rmmod $i
done

rmmod netmap_lin

for i in $MODULES
do
    modprobe $i
done

#ifconfig eth0 up
ifconfig eth4 up
#dhclient eth0
dhclient eth4
