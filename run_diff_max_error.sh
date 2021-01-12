#!/bin/bash

dataset=$1
for i in `seq 1 3`
do
  for((error=1;error<=65536;error=error*2));
  do
    ./build/diff-max-error-exp $dataset $error >> diff_max_error.txt
  done
done
