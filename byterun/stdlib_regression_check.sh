#!/usr/bin/env bash

dune build

prefix="../stdlib/regression/"
suffix=".lama"

compiler=../_build/default/src/Driver.exe

echo "Used compiler path:"
echo $compiler

echo "Build modules:"
for mod in ../stdlib/*.lama; do 
  mod_path="${mod%.*}"
  mod_file="${mod_path##*/}"
  echo $mod_path: $mod_file
  if [ ! -f $mod_file.bc ]; then
    $compiler -b $mod  -I ../stdlib/
  fi
done

echo "Run tests:"
for test in ../stdlib/regression/*.lama; do
  echo $test
  $compiler -b  $test -I ../stdlib/ > /dev/null
  test_path="${test%.*}"
  test_file="${test_path##*/}"
  echo $test_path: $test_file
  echo "" | ./byterun.exe -p $test_file.bc > test.bc.code
  # echo "" | ./byterun.exe -p $test_file.bc
  # echo "" | ./byterun.exe -vi $test_file.bc
  echo "" | ./byterun.exe -vi $test_file.bc > test.log
  # sed '1d;s/^..//' $test_file.t > test_orig.log
  # diff test.log test_orig.log

  rm $test_file.bc
  # rm test.log test_orig.log
  echo "done"
done

rm *.o
