#!/bin/bash

set -e

scripts/format.sh
scripts/clean.sh
scripts/gcc-build.sh
scripts/clean.sh
scripts/test.sh
scripts/tidy.sh
scripts/pdf.sh
scripts/font.sh

pre-commit run --all-files
