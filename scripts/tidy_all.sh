#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/build"

if ! command -v clang-tidy >/dev/null 2>&1; then
  echo "clang-tidy not found; please install it first." >&2
  exit 1
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null

# Prefer the tidy-fix target if configured in CMake.
if cmake --build "$BUILD_DIR" --target tidy-fix >/dev/null 2>&1; then
  echo "Ran clang-tidy tidy-fix target."
  exit 0
fi

# Fallback: run clang-tidy over all sources in the repo root.
mapfile -t FILES < <(git -C "$ROOT_DIR" ls-files '*.cpp' '*.hpp' 2>/dev/null || find "$ROOT_DIR" -type f \( -name '*.cpp' -o -name '*.hpp' \))
clang-tidy -p "$BUILD_DIR" "${FILES[@]}" -fix -format-style=file
echo "clang-tidy completed over ${#FILES[@]} files."

