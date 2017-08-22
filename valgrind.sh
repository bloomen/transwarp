#!/bin/bash
set -e
valgrind --quiet --leak-check=yes --error-exitcode=1 $1
