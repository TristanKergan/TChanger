#!/usr/bin/env bash

# Kütüphanenizin mutlak yolu (Absolute Path)
SO_PATH="./build/libHamzex.so"

# Root yetkisi kontrolü
if [ "$EUID" -ne 0 ]; then
  echo "[-] Hata: Bu betik root yetkisiyle (sudo) çalıştırılmalıdır!"
  echo "    Kullanım: sudo ./inject.sh"
  exit 1
fi

# CS2'nin çalışıp çalışmadığını kontrol et
PID=$(pidof cs2)

if [ -z "$PID" ]; then
  echo "[-] CS2 süreci bulunamadı! Lütfen önce oyunu başlatın."
  exit 1
fi

echo "[+] CS2 tespit edildi! PID: $PID"

# Kütüphane dosyasının varlığını kontrol et
if [ ! -f "$SO_PATH" ]; then
  echo "[-] Hata: '$SO_PATH' konumunda .so dosyası bulunamadı!"
  echo "    Lütfen yolu kontrol edin veya projeyi derlediğinizden emin olun."
  exit 1
fi

echo "[+] Enjeksiyon başlatılıyor..."

# GDB komutunu çalıştır ve gereksiz stdout/stderr uyarılarını filtrele
gdb -batch-silent \
    -p "$PID" \
    -ex "set sysroot /proc/$PID/root" \
    -ex "call (void*)dlopen(\"$SO_PATH\", 2)" \
    2>&1 | grep -E -v "warning|symbol|sysroot|vsyscall|target:|Error reading"

echo "[+] İşlem tamamlandı! Oyunu kontrol edebilirsiniz."
