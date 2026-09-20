#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# Clean build dir if CMakeCache.txt points to a different directory
if [ -f "$BUILD_DIR/CMakeCache.txt" ]; then
    CACHE_SRC="$(grep -m1 "CMAKE_CACHEFILE_DIR:INTERNAL=" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null | cut -d= -f2 || true)"
    if [ -n "$CACHE_SRC" ] && [ "$CACHE_SRC" != "$BUILD_DIR" ]; then
        echo "[*] Stale CMake cache detected, cleaning $BUILD_DIR..."
        rm -rf "$BUILD_DIR"
    fi
fi

cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -G Ninja
cmake --build "$BUILD_DIR"
