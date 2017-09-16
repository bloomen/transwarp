#!/bin/bash
set -e
thisdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

rm -rf build
mkdir build 
cd build

mode=$1
if [ "$mode" != "Debug" ];then
    mode="Release"     
fi

cmake -DCMAKE_BUILD_TYPE=$1 $thisdir
make -j4 VERBOSE=1

app=./transwarp_test
echo "+++ Running $app ..."
count=0
while [ $count -lt 100 ]; do
    $app --use-colour no --order rand --rng-seed 'time' > /dev/null
    let count+=1
done
$thisdir/valgrind.sh $app --use-colour no
echo "Tests OK"

echo "+++ Running examples"
./basic_with_three_tasks
./benchmark_simple
./benchmark_statistical
./single_thread_lock_free
./statistical_key_facts
echo "Examples OK"

cd ..
rm -rf build 
echo "All OK"
