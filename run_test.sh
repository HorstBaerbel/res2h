#!/bin/bash
rm -rf results
mkdir results
printf '%s\n' '---------------------------------------------------------------------'
./res2h ./test ./results -s -v -h ./results/resources.h -u ./results/resources.cpp
printf '%s\n' '---------------------------------------------------------------------'
./res2h ./test ./results/data.bin -b -s -v
printf '%s\n' '---------------------------------------------------------------------'
./res2hdump ./results/data.bin ./results -f -v
