#!/bin/bash
source `dirname $0`/checks.sh

matches "`find /`" "/"
matches "`find .`" "."

success

