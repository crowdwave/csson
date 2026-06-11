#!/usr/bin/env bash
# Build the CSSON core via CMake (C23-mandatory, strict warnings, lexbor fetched).
# Thin convenience wrapper — CMake is the canonical build system (see CMakeLists.txt).
# Extra args pass through, e.g.:  ./build.sh -DCSSON_SANITIZE=ON -DCSSON_TIDY=ON
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build -G Ninja "$@"
cmake --build build
echo "Done: $(pwd)/build/csson  +  $(pwd)/build/libcsson.a   (ctest: cmake --build build && ctest --test-dir build)"
