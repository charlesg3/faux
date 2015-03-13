#!/bin/bash
if [ -z "$HARE_ROOT" ] ; then
    HARE_ROOT=.
fi
grep $1 $HARE_ROOT/.config | sed 's/.*=//'

