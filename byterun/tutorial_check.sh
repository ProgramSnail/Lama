#!/usr/bin/env bash

xmake build
cp "build/linux/x86_64/release/byterun" byterun.exe

# dune build > /dev/null

prefix="../regression/"
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

for test in ../tutorial/*.lama; do 
  echo $test
  $compiler -b  $test -I ../stdlib/
  test_path="${test%.*}"
  test_file="${test_path##*/}"
  echo $test_path: $test_file
  echo " " | ./byterun.exe -vi $test_file.bc
  echo "" | ./byterun.exe -vi $test_file.bc > test.log
  rm $test_file.bc
  echo "done"
done

rm *.o

