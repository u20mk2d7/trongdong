#!/usr/bin/env bash
set -euo pipefail

echo "=== Cleaning previous build ==="
rm -rf build

echo "=== Formatting source code ==="
find ./ -type f \( \
    -name "*.cpp" -o \
    -name "*.h"   -o \
    -name "*.c"   -o \
    -name "*.hpp" -o \
    -name "*.cxx" -o \
    -name "*.cc" \
\) -exec clang-format -i --style=LLVM {} +

echo "=== Installing Conan dependencies ==="
conan install . --build=missing --profile=default

echo "=== Configuring CMake via Preset ==="
cmake --preset conan-release

echo "=== Compiling Project ==="
cmake --build --preset conan-release --parallel

echo "=== Running Binary ==="
./build/Release/kimngwu/dnse_client
