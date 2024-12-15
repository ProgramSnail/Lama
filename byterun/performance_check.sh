#!/usr/bin/env bash

dune build > /dev/null

echo "Interpreter:"
time echo '0' | lamac -i ../performance/Sort.lama > /dev/null

echo "Stack Machine:"
time echo '0' | lamac -s ../performance/Sort.lama > /dev/null


lamac -b  ../performance/Sort.lama > /dev/null

# ./byterun.exe -p Sort.bc

echo "Old Byterun:"
time ./old_byterun.exe -i Sort.bc > /dev/null

echo "Byterun:"
time ./byterun.exe -vi Sort.bc > /dev/null

echo "Byterun (verefication only):"
time ./byterun.exe -v Sort.bc > /dev/null

echo "Byterun (run only):"
time ./byterun.exe -i Sort.bc > /dev/null

rm Sort.*
rm *.o
