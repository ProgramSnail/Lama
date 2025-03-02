#!/usr/bin/env bash

dune build > /dev/null

prefix="../regression/"
suffix=".lama"

compiler=../_build/default/src/Driver.exe

echo "Used compiler path:"
echo $compiler

for test in ../regression/*.lama; do 
  echo $test
  $compiler -b  $test > /dev/null
  test_file="${test%.*}"
  echo $test_file
  cat $test_file.input | ./byterun.exe -p test*.bc > /dev/null
  cat $test_file.input | ./byterun.exe -vi test*.bc > /dev/null
  rm test*.bc
  echo "done"
done

rm *.o
