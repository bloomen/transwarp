#!/bin/bash
set -e
thisdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

modes='debug release'

for mode in $modes;do
    echo "+++ Checking "$mode" ..."

    # Checking tests
    $thisdir/src/compile_test.sh $mode
    echo "+++ Running "$thisdir/src/test.$mode" ..."
    $thisdir/src/test.$mode

    # Checking examples
    examples=$(ls $thisdir/examples/*.cpp)
    for ex in $examples; do
        $thisdir/examples/compile_example.sh $ex $mode
        echo "+++ Running "${ex%.cpp}.$mode" ..."
        ${ex%.cpp}.$mode
    done
done

echo "+++ Done!"
