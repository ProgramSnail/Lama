#!/usr/bin/env bash

dune build > /dev/null

prefix="../regression/"
suffix=".lama"

for test in ../regression/*.lama; do 
  echo $test
  ../_build/default/src/Driver.exe -b  $test > /dev/null
  test_file="${test%.*}"
  echo $test_file
  cat $test_file.input | ./byterun.exe -vi test*.bc > /dev/null
  rm test*.bc
  echo "done"
done

rm *.o

