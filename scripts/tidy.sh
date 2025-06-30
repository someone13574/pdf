#!/bin/sh

find . -iname '*.c' | xargs clang-tidy --config-file=./.clang-tidy -p build
