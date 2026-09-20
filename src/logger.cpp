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

export enum class LogLevel : int {
    Debug = 0,
    Info = 1,
    Warn = 2,
    Error = 3
};

export class Logger {
public:
    static void SetLogLevel(LogLevel level) noexcept { s_minLevel = level; }
    static LogLevel GetLogLevel() noexcept { return s_minLevel; }

    void debug(const std::string& message) {
        if (s_minLevel <= LogLevel::Debug) write("[DEBUG]", message);
    }
    void info(const std::string& message) {
        if (s_minLevel <= LogLevel::Info) write("[INFO] ", message);
    }
    void warn(const std::string& message) {
        if (s_minLevel <= LogLevel::Warn) write("[WARN] ", message);
    }
    void error(const std::string& message) {
        if (s_minLevel <= LogLevel::Error) write("[ERROR]", message);
    }

private:
    static inline LogLevel s_minLevel = [] {
        const char* env = std::getenv("HAMZEX_DEBUG");
        if (env && (*env == '1' || *env == 'y' || *env == 'Y'))
            return LogLevel::Debug;
        return LogLevel::Info;
    }();

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
            // Check for 2MB log rotation
            file.seekp(0, std::ios::end);
            if (file.tellp() > 2 * 1024 * 1024) {
                file.close();
                const std::string path = GetLogFilePath();
                const std::string rotPath = path + ".1";
                unlink(rotPath.c_str());
                (void)std::rename(path.c_str(), rotPath.c_str());
                file.open(path, std::ios::out | std::ios::trunc);
            }
        }

        if (file.is_open()) {
            file << tb << " " << level << " " << message << "\n";
            // Flush only on errors to avoid blocking the hot path
            if (level[1] == 'E') {
                file.flush();
            }
        }
    }
};
