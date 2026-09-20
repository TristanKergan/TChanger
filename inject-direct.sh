#!/usr/bin/env bash
set -euo pipefail

# Paths — independent of the working directory (script can be run from anywhere)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SO_PATH="${SO_PATH:-$SCRIPT_DIR/build/libHamzex.so}"

# Root check
if [ "$EUID" -ne 0 ]; then
  echo "[-] Hata: Bu betik root yetkisiyle (sudo) çalıştırılmalıdır!" >&2
  echo "    Kullanım: sudo ./inject-direct.sh" >&2
  exit 1
fi

# Find CS2 PID (first one if multiple)
PIDS="$(pidof cs2 2>/dev/null || true)"
PID="${PIDS%% *}"
if [ -z "$PID" ]; then
  echo "[-] CS2 süreci bulunamadı! Lütfen önce oyunu başlatın." >&2
  exit 1
fi
echo "[+] CS2 tespit edildi! PID: $PID"

# .so existence check on host
if [ ! -f "$SO_PATH" ]; then
  echo "[-] Hata: '$SO_PATH' konumunda .so dosyası bulunamadı!" >&2
  echo "    Lütfen önce './build.sh' ile projeyi derleyin." >&2
  exit 1
fi

# .so check — verify it is visible inside the game's mount namespace (pressure-vessel)
if [ ! -f "/proc/$PID/root$SO_PATH" ]; then
  echo "[-] Hata: '$SO_PATH' oyunun mount namespace'i içinde görünmüyor!" >&2
  echo "    Kapsayıcının erişebileceği bir yol kullanın veya SO_PATH ayarlayın." >&2
  exit 1
fi

echo "[+] Enjeksiyon başlatılıyor..."

REAL_SO="$(realpath "$SO_PATH" 2>/dev/null || echo "$SO_PATH")"
SO_BASENAME="$(basename "$SO_PATH")"

GDB_OUT="$(gdb -batch -p "$PID" \
    -ex "set sysroot /proc/$PID/root" \
    -ex "call (void*)dlopen(\"$SO_PATH\", 2)" \
    -ex "call (char*)dlerror()" 2>&1 || true)"

# Verify whether dlopen actually succeeded via /proc/PID/maps (exact path, realpath, or DSO name)
if grep -qF "$SO_PATH" /proc/$PID/maps 2>/dev/null || \
   grep -qF "$REAL_SO" /proc/$PID/maps 2>/dev/null || \
   grep -qE "/$SO_BASENAME(\s|$)" /proc/$PID/maps 2>/dev/null; then
  echo "[+] Enjeksiyon başarılı: libHamzex.so oyuna yüklendi"
  echo "[+] İşlem tamamlandı! Oyunu kontrol edebilirsiniz."
else
  echo "[-] Enjeksiyon başarısız oldu (libHamzex.so haritalanamadı)." >&2
  printf '%s\n' "$GDB_OUT" | grep -Ev 'warning|Reading symbols|Error reading|sysroot|vsyscall|target:|Thread|Inferior' || true
  exit 1
fi
