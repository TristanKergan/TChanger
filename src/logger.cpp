module;
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <mutex>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

export module Logger;

namespace {

bool EnsureDir(const std::string& dir) {
    struct stat st{};
    if (stat(dir.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return mkdir(dir.c_str(), 0700) == 0;
}

std::string GetLogFilePath() {
    // 1. $XDG_STATE_HOME/hamzex/hamzex.log
    const char* xdg_state = std::getenv("XDG_STATE_HOME");
    if (xdg_state && *xdg_state) {
        std::string dir = std::string(xdg_state) + "/hamzex";
        if (EnsureDir(dir))
            return dir + "/hamzex.log";
    }

    // 2. $HOME/.local/state/hamzex/hamzex.log
    const char* home = std::getenv("HOME");
    if (home && *home) {
        std::string local = std::string(home) + "/.local";
        EnsureDir(local);
        std::string state = local + "/state";
        EnsureDir(state);
        std::string dir = state + "/hamzex";
        if (EnsureDir(dir))
            return dir + "/hamzex.log";
        std::string config_dir = std::string(home) + "/.config/hamzex";
        if (EnsureDir(config_dir))
            return config_dir + "/hamzex.log";
    }

    // 3. Fallback: /tmp/hamzex-<uid>/hamzex.log (isolated per user)
    std::string tmp_dir = "/tmp/hamzex-" + std::to_string(getuid());
    EnsureDir(tmp_dir);
    return tmp_dir + "/hamzex.log";
}

}  // namespace

export class Logger {
public:
    void info(const std::string& message)  { write("[INFO] ", message); }
    void warn(const std::string& message)  { write("[WARN] ", message); }
    void error(const std::string& message) { write("[ERROR]", message); }

private:
    void write(const char* level, const std::string& message) {
        static std::mutex mtx;
        static std::ofstream file = [] {
            const std::string path = GetLogFilePath();
            return std::ofstream(path, std::ios::out | std::ios::app);
        }();

        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf{};
        localtime_r(&t, &tm_buf);
        char tb[32] = {0};
        std::strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", &tm_buf);

        std::lock_guard<std::mutex> lk(mtx);
        if (file.is_open()) {
            file << tb << " " << level << " " << message << "\n";
            // Flush only on errors to avoid blocking the hot path
            if (level[1] == 'E') {
                file.flush();
            }
        }
    }
};
