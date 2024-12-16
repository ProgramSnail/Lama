#!/usr/bin/env bash

dune build > /dev/null

prefix="../regression/"
suffix=".lama"

for test in ../regression/*.lama; do 
  echo $test
  lamac -b  $test > /dev/null
  ./byterun.exe -v test*.bc > /dev/null
  rm test*.bc
  echo "done"
done

rm test.bc
rm *.o

