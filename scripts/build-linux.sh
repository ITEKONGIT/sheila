#!/usr/bin/env sh

set -eu

if [ -n "${VCPKG_ROOT:-}" ]; then
    cmake --preset linux \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
else
    cmake --preset linux
fi
cmake --build --preset linux
ctest --test-dir build/linux --output-on-failure

printf '%s\n' 'Linux build ready at build/linux/ssheila'
