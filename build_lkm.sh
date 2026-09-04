#!/usr/bin/env bash
set -euo pipefail

# hide_tracer LKM build script — independent of the working directory (CWD)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LKM_DIR="$SCRIPT_DIR/lkm_src"
MODULE_NAME="hide_tracer"

usage() {
  echo "Usage: $0 [build|clean]" >&2
  echo "  build (default): compile the kernel module, then prune build artifacts" >&2
  echo "  clean: remove all build artifacts (including .ko)" >&2
}

prune_artifacts() {
  rm -f "$LKM_DIR"/.*.cmd \
        "$LKM_DIR"/*.o \
        "$LKM_DIR"/*.mod \
        "$LKM_DIR"/*.mod.c \
        "$LKM_DIR"/modules.order \
        "$LKM_DIR"/Module.symvers \
        "$LKM_DIR"/.module-common.o
}

case "${1:-build}" in
  build)
    make -C "$LKM_DIR"
    prune_artifacts
    [ -f "$LKM_DIR/$MODULE_NAME.ko" ] && echo "[+] Built: $LKM_DIR/$MODULE_NAME.ko"
    ;;
  clean)
    make -C "$LKM_DIR" clean
    prune_artifacts
    echo "[+] Cleaned: $LKM_DIR"
    ;;
  *)
    usage
    exit 1
    ;;
esac