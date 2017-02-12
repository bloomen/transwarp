#!/bin/bash
set -e
thisdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

compiler=g++
if [ "x$1" != "x" ];then
	compiler=$1
fi
export CXX=$compiler

modes='debug release'

for mode in $modes;do
    echo "+++ Checking $mode with $compiler ..."

    # Checking tests
    $thisdir/src/compile_test.sh $mode
    echo "+++ Running $thisdir/src/test.$mode ..."
    $thisdir/src/test.$mode

    # Checking examples
    examples=$(ls $thisdir/examples/*.cpp)
    for ex in $examples; do
        $thisdir/examples/compile_example.sh $ex $mode
        echo "+++ Running ${ex%.cpp}.$mode ..."
        ${ex%.cpp}.$mode
    done
done

echo "+++ Done!"
