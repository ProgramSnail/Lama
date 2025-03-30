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
  # cat $test_file.input | ./byterun.exe -p test*.bc > test.bc.code
  # cat $test_file.input | ./byterun.exe -p test*.bc
  # cat $test_file.input | ./byterun.exe -vi test*.bc
  cat $test_file.input | ./byterun.exe -vi test*.bc > test.log
  sed '1d;s/^..//' $test_file.t > test_orig.log
  diff test.log test_orig.log

  rm test*.bc
  rm test.log test_orig.log
  echo "done"
done

rm *.o
