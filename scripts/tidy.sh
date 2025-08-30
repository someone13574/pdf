#!/bin/sh

echo "Running clang-tidy..."
find . -iname '*.c' | xargs clang-tidy -extra-arg="-UTEST" --config-file=./.clang-tidy -p build
