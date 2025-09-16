#!/usr/bin/env bash
set -euo pipefail
OPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-$(brew --prefix openssl@3)}"
cmake -S . -B build -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR"
cmake --build build
CTEST_OUTPUT_ON_FAILURE=1 ctest --test-dir build -j"$(sysctl -n hw.ncpu)"
