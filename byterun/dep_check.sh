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
  test_path="${test%.*}"
  test_file="${test_path##*/}"
  echo $test_path: $test_file
  cat $test_path.input | ./byterun.exe -vi $test_file.bc
  rm $test_file.bc
  echo "done"
done

rm *.o
