#!/bin/bash
set -e
thisdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

examples=$(ls $thisdir/*.dot)
for ex in $examples; do
    echo "+++ Creating graph from $ex ..."
    dot -Tpng $ex -o ${ex%.dot}.png
done
