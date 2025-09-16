#!/usr/bin/env bash
set -e
OPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-$(brew --prefix openssl@3)}"
cmake -S . -B build -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR"
cmake --build build
./build/gitfly "$@"
