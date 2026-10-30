#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
lab_root=$PWD
if ! command -v x86_64-w64-mingw32-gcc >/dev/null; then
  # Rootless Ubuntu fallback; all downloads and extraction stay under build/.
  mkdir -p build/toolchain-debs build/mingw
  if [[ ! -x build/mingw/usr/bin/x86_64-w64-mingw32-gcc-posix ]]; then
    (cd build/toolchain-debs && apt-get download \
      gcc-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix-runtime \
      gcc-mingw-w64-base mingw-w64-x86-64-dev mingw-w64-common binutils-mingw-w64-x86-64)
    for deb in build/toolchain-debs/*.deb; do dpkg-deb -x "$deb" build/mingw; done
  fi
  ln -sf x86_64-w64-mingw32-gcc-posix build/mingw/usr/bin/x86_64-w64-mingw32-gcc
  export PATH="$lab_root/build/mingw/usr/bin:$PATH"
fi
cmake -S host -B build/host-win -G Ninja -DCMAKE_TOOLCHAIN_FILE="$lab_root/cmake/mingw-w64.cmake"
cmake --build build/host-win
