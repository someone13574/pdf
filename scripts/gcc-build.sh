#!/bin/bash

set -e

scripts/clean.sh
CC=gcc CXX=g++ cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j 8
