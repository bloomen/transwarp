#!/bin/bash
set -e
if [ "$(uname)" = "Linux" ];then  # valgrind currently buggy on Mac
res=0
echo "+++ Valgrinding $* ..."
valgrind --quiet --tool=memcheck --leak-check=full --show-reachable=yes \
--num-callers=20 --error-exitcode=1 $* || res=1
if [ $res -eq 1 ];then
    echo "Valgrind failed"
    exit 1  
fi
echo "Valgrind OK"
fi
