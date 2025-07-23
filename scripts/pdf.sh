#!/bin/bash

set -e

CC=clang CXX=clang++ cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
echo "----------------"
build/examples/pdf-example "$@"
