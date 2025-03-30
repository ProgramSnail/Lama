#!/usr/bin/env bash

dune build

prefix="../stdlib/regression/"
suffix=".lama"

compiler=../_build/default/src/Driver.exe

echo "Used compiler path:"
echo $compiler

echo "Build modules:"
for mod in ../stdlib/*.lama; do 
  echo $mod
  $compiler -b $mod  -I ../stdlib/
done

echo "Run tests:"
for test in ../stdlib/regression/*01.lama; do 
  echo $test
  $compiler -b  $test -I ../stdlib/ > /dev/null
  test_file="${test%.*}"
  echo $test_file
  # cat $test_file.input | ./byterun.exe -p test*.bc > test.bc.code
  # cat $test_file.input | ./byterun.exe -p test*.bc
  echo "" | ./byterun.exe -vi test*.bc
  echo "" | ./byterun.exe -vi test*.bc > test.log
  # sed '1d;s/^..//' $test_file.t > test_orig.log
  # diff test.log test_orig.log

  rm test*.bc
  # rm test.log test_orig.log
  echo "done"
done

rm *.bc
rm *.o
