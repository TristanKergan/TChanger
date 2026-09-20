#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
import Logger;
import FsnHook;
import Config;
import SkinConfig;
import RuntimeConfig;
import IpcServer;

static Logger Log;

enum class LifecycleState {
    Uninitialized,
    Initializing,
    Ready,
    Failed,
    Stopping,
    Stopped
};

static std::atomic<LifecycleState> g_lifecycle{LifecycleState::Uninitialized};
static std::atomic<bool> g_stopRequested{false};
static std::thread g_initThread;

void initializeWorker() {
    Log.info("Initialize: worker started");
    const auto timer = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    int attempts = 0;

    while (!g_stopRequested.load(std::memory_order_relaxed)) {
        if (FsnHook::Install()) {
            Log.info("Initialize: FSN hook installed successfully");
            runtimeconfig::RuntimeConfig::Instance().LoadFromFile();
            hamzex::server::IpcServer::Global().Start();
            g_lifecycle.store(LifecycleState::Ready, std::memory_order_release);
            return;
        }

        if (++attempts % 10 == 0) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "initialize: retry %d", attempts);
            Log.warn(buf);
        }

        if (std::chrono::steady_clock::now() >= timer) {
            Log.warn("Initialize: FSN hook install timed out");
            g_lifecycle.store(LifecycleState::Failed, std::memory_order_release);
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    Log.info("Initialize: worker stopped before completing");
}

// Kütüphane yüklendiğinde çalışır (LOAD)
__attribute__((constructor))
void on_load() {
    Log.info("Hamzex Loaded (LOAD)");
    g_lifecycle.store(LifecycleState::Initializing, std::memory_order_release);
    g_stopRequested.store(false, std::memory_order_relaxed);
    g_initThread = std::thread(initializeWorker);
}

// Kütüphane unload olunca çalışır (SHUTDOWN)
__attribute__((destructor))
void on_unload(void) {
    Log.info("Hamzex Unloading (STOPPING)");
    g_lifecycle.store(LifecycleState::Stopping, std::memory_order_release);
    g_stopRequested.store(true, std::memory_order_relaxed);

    if (g_initThread.joinable()) {
        g_initThread.join();
    }

    hamzex::server::IpcServer::Global().Stop();
    FsnHook::Uninstall();
    g_lifecycle.store(LifecycleState::Stopped, std::memory_order_release);
    Log.info("Hamzex Unloaded (STOPPED)");
}
