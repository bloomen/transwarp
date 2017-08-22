#!/bin/bash
set -e
thisdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cppcheck --template="{file};{line};{severity};{id};{message}" --quiet --enable=all \
--std=c99 --std=c++11 --error-exitcode=1 --relative-paths --suppress=missingIncludeSystem \
--inline-suppr $thisdir
