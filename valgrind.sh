#!/bin/bash
set -e
if [ "$(uname)" = "Linux" ];then  # valgrind currently buggy on Mac
valgrind --quiet --tool=memcheck --leak-check=full --show-reachable=yes \
--num-callers=20 --error-exitcode=1 $1
fi
