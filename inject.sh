#!/usr/bin/env bash
set -euo pipefail

# Paths — independent of the working directory (script can be run from anywhere)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

SO_PATH="${SO_PATH:-$SCRIPT_DIR/build/libHamzex.so}"
LKM_PATH="$SCRIPT_DIR/lkm_src/hide_tracer.ko"
LKM_NAME="hide_tracer"

# Root check
if [ "$EUID" -ne 0 ]; then
  echo "[-] Error: This script must be run as root!" >&2
  echo "    Usage: sudo ./inject.sh" >&2
  exit 1
fi

# Find CS2 PID (first one if multiple)
PIDS="$(pidof cs2 2>/dev/null || true)"
PID="${PIDS%% *}"
if [ -z "$PID" ]; then
  echo "[-] CS2 process not found! Please start the game first." >&2
  exit 1
fi
echo "[+] CS2 detected! PID: $PID"

# .so check — verify it is actually visible inside the game's mount namespace.
# CS2 runs under pressure-vessel (a container): host paths like /lib can be
# shadowed by the runtime's own libraries, so a file that exists on the host
# may not resolve inside the game (dlopen would return NULL).
if [ ! -f "/proc/$PID/root$SO_PATH" ]; then
  echo "[-] Error: '$SO_PATH' is not visible inside the game's mount namespace!" >&2
  echo "    Use a path the container can see (e.g. the repo build dir) or set SO_PATH." >&2
  exit 1
fi

# LKM check — building is not this script's job (run make in lkm_src)
if [ ! -f "$LKM_PATH" ]; then
  echo "[-] Error: '$LKM_PATH' not found! Build it first with './build_lkm.sh'." >&2
  exit 1
fi

# Always unload the module on exit (even on error)
LKM_LOADED=0
cleanup() {
  if [ "$LKM_LOADED" = 1 ]; then
    rmmod "$LKM_NAME" 2>/dev/null || \
      echo "[!] Warning: rmmod failed, remove it manually: sudo rmmod $LKM_NAME" >&2
  fi
}
trap cleanup EXIT

# --- 1. Load TracerPid masking module ---
echo "[+] Loading TracerPid masking module (insmod)..."
insmod "$LKM_PATH"
LKM_LOADED=1

# --- 2. Inject with GDB (result verified against /proc/PID/maps) ---
echo "[+] Starting GDB injection..."
# gdb must auto-load libc to resolve dlopen for `call`; the explicit
# `sharedlibrary` route does not work inside the pressure-vessel container.
# Output is captured and only shown if the injection fails, so the solib
# read warnings stay hidden on success.
GDB_OUT="$(gdb -batch-silent -p "$PID" \
    -ex "set sysroot /proc/$PID/root" \
    -ex "call (void*)dlopen(\"$SO_PATH\", 2)" 2>&1 || true)"

# gdb's '$1 = (void *) 0x...' result is not a reliable success signal here, so
# the ground truth is /proc/PID/maps: dlopen succeeded iff the .so is mapped.
if grep -qF "$SO_PATH" /proc/$PID/maps 2>/dev/null; then
  echo "[+] Injection successful: libHamzex.so is loaded into the game"
  echo "[+] Done."
else
  echo "[-] Injection failed (libHamzex.so not mapped in the game)." >&2
  printf '%s\n' "$GDB_OUT" | \
      grep -Ev 'warning|Reading symbols|Error reading|sysroot|vsyscall|target:' || true
  exit 1
fi
