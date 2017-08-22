#!/bin/bash
set -e
thisdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

compiler=g++
if [ "x$1" != "x" ];then
    compiler=$1
fi
export CXX=$compiler

modes='debug release'

echo "+++ Running CppCheck ..."
$thisdir/cppcheck.sh
echo "CppCheck OK"

for mode in $modes;do
    echo "+++ Checking $mode with $compiler ..."

    # Checking tests
    $thisdir/src/compile_test.sh $mode
    app=$thisdir/src/test.$mode
    echo "+++ Running $app ..."
    count=0
    while [ $count -lt 100 ]; do
        $app > /dev/null
        let count+=1
    done
    echo "+++ Valgrinding $app ..."
    $thisdir/valgrind.sh $app
    echo "Tests OK"

    # Checking examples
    examples=$(ls $thisdir/examples/*.cpp)
    for ex in $examples; do
        $thisdir/examples/compile_example.sh $ex $mode
        app=${ex%.cpp}.$mode
        echo "+++ Running $app ..."
        $app
        if [[ $app != *"benchmark_"* ]]; then
            echo "+++ Valgrinding $app ..."
            $thisdir/valgrind.sh $app
        fi
    done
    echo "Examples OK"
       
done

echo "+++ Done!"
