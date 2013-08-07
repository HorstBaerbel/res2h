@echo off
echo ---------------------------------------------------------------------
mkdir results
echo ---------------------------------------------------------------------
.\Release\res2h.exe .\test .\results -s -v -h .\results\resources.h -u .\results\resources.cpp
echo ---------------------------------------------------------------------
.\Release\res2h.exe .\test .\results\data.bin -b -s -v
echo ---------------------------------------------------------------------
.\Release\res2hdump.exe .\results\data.bin .\results -f -v