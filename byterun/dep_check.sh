#!/usr/bin/env bash

dune build > /dev/null

prefix="../regression/"
suffix=".lama"

compiler=../_build/default/src/Driver.exe

echo "Used compiler path:"
echo $compiler

$compiler -b  regression/Dep.lama > /dev/null

for test in regression/dep_test*.lama; do 
  echo $test
  $compiler -b  $test -I regression/ > /dev/null
  test_file="${test%.*}"
  echo $test_file
  cat $test_file.input | ./byterun.exe -vi dep_test*.bc > /dev/null
  rm dep_test*.bc
  echo "done"
done

rm Dep.bc
rm *.o
