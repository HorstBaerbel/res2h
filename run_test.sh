#!/bin/bash
printf "---------------------------------------------------------------------\n"
mkdir results
printf "---------------------------------------------------------------------\n"
./res2h ./test ./results -s -v -h ./results/resources.h -u ./results/resources.cpp
printf "---------------------------------------------------------------------\n"
./res2h ./test ./results/data.bin -b -s -v
printf "---------------------------------------------------------------------\n"
./res2hdump ./results/data.bin ./results -f -v
