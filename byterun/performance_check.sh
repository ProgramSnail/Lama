#!/usr/bin/env bash

dune build > /dev/null

echo "Interpreter:"
time echo '0' | lamac -i ../performance/Sort.lama > /dev/null

echo "Stack Machine:"
time echo '0' | lamac -s ../performance/Sort.lama > /dev/null


lamac -b  ../performance/Sort.lama > /dev/null

echo "Byterun:"
time ./byterun.exe Sort.bc > /dev/null

rm Sort.*
