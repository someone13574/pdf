#!/bin/bash

echo "Formatting project..."
find . -type f \( -iname '*.c' -o -iname '*.h' -o -iname '*.cpp' -o -iname '*.hpp' \) -print0 \
  | xargs -0 clang-format -i
