#!/bin/bash
mkdir results
./res2h ./test ./results -s -v -h ./results/resources.h -u ./results/resources.cpp
./res2h ./test ./results/data.bin -b -s -v
./res2hdump ./results/data.bin ./results -f -v
