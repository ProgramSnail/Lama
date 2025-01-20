#!/usr/bin/env bash

dune build > /dev/null

prefix="../regression/"
suffix=".lama"

compiler=../_build/default/src/Driver.exe

echo "Used compiler path:"
echo $compiler

$compiler -b  regression/Dep.lama
$compiler -b  -I regression/ regression/Dep2.lama

$compiler -b  ../stdlib/List.lama
$compiler -b  ../stdlib/Ref.lama
$compiler -b  ../stdlib/Fun.lama

for test in regression/dep_test*.lama; do 
  echo $test
  $compiler -b  $test -I regression/
  test_file="${test%.*}"
  echo $test_file
  cat $test_file.input | ./byterun.exe -vi dep_test*.bc
  rm dep_test*.bc
  echo "done"
done

rm *.bc
rm *.o
