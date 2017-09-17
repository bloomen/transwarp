#!/bin/bash
set -e
thisdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
app=$1
echo "+++ Running $app ..."
count=0
while [ $count -lt 100 ]; do
    $app --use-colour no --order rand --rng-seed 'time' > /dev/null
    let count+=1
done
$thisdir/valgrind.sh $app --use-colour no
echo "Tests OK"
