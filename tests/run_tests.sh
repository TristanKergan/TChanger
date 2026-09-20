#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"
mkdir -p build

echo "[*] Compiling Hamzex test modules with GCC 16 C++26..."
g++ -std=c++26 -fmodules-ts -O2 -c src/logger.cpp -o build/test_logger.o
g++ -std=c++26 -fmodules-ts -O2 -c src/itemcatalog.cpp -o build/test_itemcatalog.o
g++ -std=c++26 -fmodules-ts -O2 -c src/json.cpp -o build/test_json.o
g++ -std=c++26 -fmodules-ts -O2 -c src/config.cpp -o build/test_config.o
g++ -std=c++26 -fmodules-ts -O2 -c src/skinconfig.cpp -o build/test_skinconfig.o
g++ -std=c++26 -fmodules-ts -O2 -c tests/test_standalone.cpp -o build/test_standalone.o

echo "[*] Linking standalone test executable..."
g++ build/test_standalone.o build/test_logger.o build/test_itemcatalog.o build/test_json.o build/test_config.o build/test_skinconfig.o -o tests/test_standalone

echo "[*] Running standalone tests..."
./tests/test_standalone
