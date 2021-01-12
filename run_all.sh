
rm read_only_result.txt
rm read_heavy_result.txt
rm write_heavy_result.txt
rm write_only_result.txt
rm read_range_write_result.txt

dataset=$1
for i in `seq 1 3`
do
  echo "bench"
  sh run.sh bench $1
  echo "b-tree"
  sh run.sh stx-bench $1
  echo "art"
  sh run.sh art-bench $1
  echo "pgm"
  sh run.sh pgm-bench $1
  echo "dpgm"
  sh run.sh dpgm-bench $1
  echo "alex"
  sh run.sh alex-bench $1
done