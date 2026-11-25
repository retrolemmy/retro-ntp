#!/bin/sh
# Builds NTPSet for 68k Macs inside the native Retro68 image (build it once:
#   docker build -t retro68-native -f mac/Dockerfile.retro68 mac ).
# Output: mac/build/NTPSet.bin (MacBinary), NTPSet.dsk (HFS disk image), NTPSet.APPL
set -e
cd "$(dirname "$0")/.."
docker run --rm --entrypoint /bin/sh -u "$(id -u):$(id -g)" -v "$(pwd):/src" retro68-native -c '
    set -e
    mkdir -p /src/mac/build && cd /src/mac/build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=/Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake >/dev/null
    make 2>&1 | grep -v "^\[" || true
    test -f NTPSet.bin
  '
ls -l mac/build/NTPSet.bin mac/build/NTPSet.dsk
