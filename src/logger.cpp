module;
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <mutex>
#include <ctime>
#include <cstdio>

export module Logger;

export class Logger {
public:
    void info(const std::string& message)  { write("[INFO] ", message); }
    void warn(const std::string& message)  { write("[WARN] ", message); }
    void error(const std::string& message) { write("[ERROR]", message); }

private:
    void write(const char* level, const std::string& message) {
        // Fonksiyon-içi statik: tüm Logger instance'ları arasında paylaşılır
        // (farklı TU'lardaki static Logger nesneleri de serileşir).
        static std::ofstream file("/tmp/hamzex.log", std::ios::out | std::ios::app);
        static std::mutex mtx;

        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        struct tm tm_buf{};
        localtime_r(&t, &tm_buf);
        char tb[32] = {0};
        std::strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", &tm_buf);

        std::lock_guard<std::mutex> lk(mtx);
        std::string out = std::string(tb) + " " + level + " " + message + "\n";
        file << out;
        file.flush();
        // std::cout << out; // for terminal
    }
};
