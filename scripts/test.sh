#!/bin/bash

set -e

CC=clang CXX=clang++ cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 8
echo "----------------"
build/libs/test/pdf-test-main "$@"
