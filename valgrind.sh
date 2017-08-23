#!/bin/bash
set -e
valgrind --quiet --tool=memcheck --leak-check=full --show-reachable=yes \
--num-callers=20 --error-exitcode=1 $1
