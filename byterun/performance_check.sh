#!/usr/bin/env bash

xmake build
cp "build/linux/x86_64/release/byterun" byterun.exe

# dune build > /dev/null

compiler=../_build/default/src/Driver.exe

echo "Used compiler path:"
echo $compiler

# echo "Interpreter:"
# time echo '0' | $compiler -i ../performance/Sort.lama

echo "Stack Machine:"
time echo '0' | $compiler -s ../performance/Sort.lama > /dev/null

$compiler -b  ../performance/Sort.lama

# ./byterun.exe -p Sort.bc

# echo "Old Byterun:"
# time ./old_byterun.exe -i Sort.bc > /dev/null

echo "Code:"
time ./byterun.exe -p Sort.bc > /dev/null

echo "Byterun:"
time ./byterun.exe -vi Sort.bc > /dev/null

# # NOTE: is not possible for now
# echo "Byterun (verefication only):"
# time ./byterun.exe -v Sort.bc > /dev/null

echo "Byterun (run only):"
time ./byterun.exe -i Sort.bc > /dev/null

rm Sort.*
rm *.o
