#!/bin/bash
set -e
thisdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cpp=$thisdir/test.cpp
examples=$thisdir/../examples/*.cpp

user_mode=$1
mode=release
opt=-O3
if [ "$user_mode" = "debug" ];then
    mode=debug
    opt=-O0
fi

echo "Compiling $cpp ..."
g++ -pthread -std=c++11 $opt -g -lunittest -DUSE_LIBUNITTEST -pedantic -pedantic-errors -Wall -Wextra -Werror -Wconversion -pthread -Wno-maybe-uninitialized $examples $cpp -o ${cpp%.cpp}.$mode
