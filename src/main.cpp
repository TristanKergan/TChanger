#include <chrono>
#include <cstdio>
#include <thread>
import Logger;
import FsnHook;
import Config;
import SkinConfig;

static Logger Log;

// Başlangıç: libclient.so yüklenip FSN vtable hook kurulana kadar bekle.
// (Sonsuz döngü değil: hook kurulunca return eder.)
void initialize() {
    Log.info("Initialize Started");
    const auto timer = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    int attempts = 0;
    while (1) {
        if (FsnHook::Install()) {
            Log.info("FSN Hook Installing");
            // Config + skin tablosunu sıcak yoldan (FSN) önce ön-yükle.
            config::Global();
            skinconfig::SkinTable::Global();
            return;
        }
        if (++attempts % 10 == 0) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "initialize: retry %d", attempts);
            Log.warn(buf);
        }
        if (std::chrono::steady_clock::now() >= timer) {
            Log.warn("FSN hook install timed out");
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Kütüphane yüklendiğinde çalışır
__attribute__((constructor))
void on_load() {
    Log.info("Hamzex Loaded");
    initialize();
}

// Kütüphane unload olunca çalışır
__attribute__((destructor))
void on_unload(void) {
    FsnHook::Uninstall();
    Log.info("Hamzex Unloaded");
}
