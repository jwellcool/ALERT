
path=$2
files=$(ls $path)
for filename in $files
do
   ./build/$1 $path/$filename ro >> read_only_result.txt
   ./build/$1 $path/$filename rh >> read_heavy_result.txt
   ./build/$1 $path/$filename sr >> small_range_result.txt
   ./build/$1 $path/$filename wh >> write_heavy_result.txt
   ./build/$1 $path/$filename wo >> write_only_result.txt
   ./build/$1 $path/$filename rrw >> read_range_write_result.txt
done


