#!/bin/sh
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -O2 -Wall -Wextra tests/test_compressor.cpp -o /tmp/test_compressor
/tmp/test_compressor
