#pragma once

#include <cerrno>
#include <cstdlib>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace hamzex::ipc {

inline constexpr std::string_view kSocketName = "hamzex_control.sock";
inline constexpr std::size_t kMaxPacketSize = 65536;

inline bool WriteAll(int fd, const void* data, std::size_t size) {
    const char* ptr = static_cast<const char*>(data);
    std::size_t remaining = size;
    while (remaining > 0) {
        ssize_t n = write(fd, ptr, remaining);
        if (n > 0) {
            ptr += n;
            remaining -= static_cast<std::size_t>(n);
        } else if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        } else {
            return false;
        }
    }
    return true;
}

inline bool EnsureDir(const std::string& path) {
    if (path.empty()) return false;
    struct stat st{};
    if (stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            chmod(path.c_str(), 0700);
            return true;
        }
        return false;
    }
    // Recursive mkdir
    std::size_t pos = 1;
    while ((pos = path.find('/', pos)) != std::string::npos) {
        std::string sub = path.substr(0, pos);
        if (stat(sub.c_str(), &st) != 0) {
            if (mkdir(sub.c_str(), 0700) != 0 && errno != EEXIST) {
                return false;
            }
        }
        ++pos;
    }
    if (mkdir(path.c_str(), 0700) == 0 || errno == EEXIST) {
        chmod(path.c_str(), 0700);
        return true;
    }
    return false;
}

inline bool IsSafeSocketPath(const std::string& path) {
    struct stat st{};
    if (lstat(path.c_str(), &st) != 0) {
        return errno == ENOENT;
    }
    if (st.st_uid != getuid()) {
        return false;
    }
    if (S_ISLNK(st.st_mode)) {
        return false;
    }
    return S_ISSOCK(st.st_mode);
}

inline std::string GetSocketPath() {
    std::string candidate;

    // 1. Check $XDG_RUNTIME_DIR (standard user-specific secure tmpfs on systemd Linux)
    const char* xdgRuntime = std::getenv("XDG_RUNTIME_DIR");
    if (xdgRuntime && *xdgRuntime) {
        candidate = std::string(xdgRuntime) + "/" + std::string(kSocketName);
    } else {
        // 2. Check $HOME/.local/state/hamzex/
        const char* home = std::getenv("HOME");
        if (home && *home) {
            std::string dir = std::string(home) + "/.local/state/hamzex";
            EnsureDir(dir);
            candidate = dir + "/" + std::string(kSocketName);
        } else {
            // 3. Fallback to /tmp/hamzex_<uid>.sock
            uid_t uid = getuid();
            candidate = "/tmp/hamzex_" + std::to_string(uid) + "_" + std::string(kSocketName);
        }
    }

    // Safety guard against sockaddr_un::sun_path overflow (typically 108 bytes on Linux)
    if (candidate.size() >= sizeof(sockaddr_un{}.sun_path) - 1) {
        candidate = "/tmp/hzx_" + std::to_string(getuid()) + ".sock";
    }

    return candidate;
}

}  // namespace hamzex::ipc
