#!/bin/bash

mutool clean test-files/test.pdf test-files/test.pdf
mutool clean -z -a test-files/test.pdf test-files/compressed.pdf
mutool convert -o test-files/embedded.pdf -O ascii test-files/test.pdf
