#!/bin/bash

cd netmap/net
make
make addmods
make scaleback-eth5
#ifconfig eth0 up
dhclient eth4
#dhclient eth0
