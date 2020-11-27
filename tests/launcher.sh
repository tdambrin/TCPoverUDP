#!/usr/bin/env bash
#serv=$1
port=$1
for i in {1..10}
do
   #echo "Welcome $i times"
   ./client $port
done