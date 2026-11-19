#!/bin/sh
# Run available portable and Linux x86-64 native differential checks.
# Requires CMake, CTest and a GCC- or Clang-compatible toolchain.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
for variant in debug release sanitize; do
    directory="build-verify-$variant"
    case "$variant" in
        debug) cmake -S . -B "$directory" -DCMAKE_BUILD_TYPE=Debug ;;
        release) cmake -S . -B "$directory" -DCMAKE_BUILD_TYPE=Release ;;
        sanitize) cmake -S . -B "$directory" -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
          -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' ;;
    esac
    cmake --build "$directory" --parallel 2
    ctest --test-dir "$directory" --output-on-failure
done
