module;
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include "ipc_protocol.hpp"

export module IpcServer;

import Logger;
import Json;
import RuntimeConfig;

export namespace hamzex::server {

class IpcServer {
public:
    static IpcServer& Global();

    bool Start();
    void Stop();
    bool IsRunning() const noexcept { return running_.load(std::memory_order_relaxed); }
    const std::string& SocketPath() const noexcept { return socketPath_; }

private:
    IpcServer() = default;
    ~IpcServer() { Stop(); }

    void WorkerLoop();
    void HandleClient(int clientFd);

    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::thread workerThread_;
    int listenFd_ = -1;
    std::string socketPath_;
};

}  // namespace hamzex::server

hamzex::server::IpcServer& hamzex::server::IpcServer::Global() {
    static IpcServer server;
    return server;
}

bool hamzex::server::IpcServer::Start() {
    if (running_.load(std::memory_order_acquire))
        return true;

    static Logger log;
    socketPath_ = hamzex::ipc::GetSocketPath();

    // 1. Stale socket check: test connect()
    int testFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (testFd >= 0) {
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);
        if (connect(testFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
            close(testFd);
            log.warn("IpcServer: active instance already listening on " + socketPath_);
            return false;
        }
        close(testFd);
    }
    if (hamzex::ipc::IsSafeSocketPath(socketPath_)) {
        unlink(socketPath_.c_str());
    }

    // 2. Create and bind listening socket
    listenFd_ = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listenFd_ < 0) {
        log.error("IpcServer: failed to create socket");
        return false;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(listenFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        log.error("IpcServer: bind failed for " + socketPath_);
        close(listenFd_);
        listenFd_ = -1;
        return false;
    }

    // Set secure user-only permissions (0600)
    chmod(socketPath_.c_str(), 0600);

    if (listen(listenFd_, 5) != 0) {
        log.error("IpcServer: listen failed");
        close(listenFd_);
        unlink(socketPath_.c_str());
        listenFd_ = -1;
        return false;
    }

    stopRequested_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);

    workerThread_ = std::thread(&IpcServer::WorkerLoop, this);
    log.info("IpcServer: listening on " + socketPath_);
    return true;
}

void hamzex::server::IpcServer::Stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel))
        return;

    stopRequested_.store(true, std::memory_order_release);

    if (listenFd_ >= 0) {
        shutdown(listenFd_, SHUT_RDWR);
        close(listenFd_);
        listenFd_ = -1;
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    if (!socketPath_.empty() && hamzex::ipc::IsSafeSocketPath(socketPath_)) {
        unlink(socketPath_.c_str());
    }

    static Logger log;
    log.info("IpcServer: stopped cleanly");
}

void hamzex::server::IpcServer::WorkerLoop() {
    while (!stopRequested_.load(std::memory_order_relaxed)) {
        struct pollfd pfd{};
        pfd.fd = listenFd_;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 250);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;

        if (pfd.revents & POLLIN) {
            int clientFd = accept4(listenFd_, nullptr, nullptr, SOCK_CLOEXEC);
            if (clientFd >= 0) {
                HandleClient(clientFd);
                close(clientFd);
            }
        }
    }
}

void hamzex::server::IpcServer::HandleClient(int clientFd) {
    std::string requestBuf;
    char chunk[4096];

    while (requestBuf.size() < hamzex::ipc::kMaxPacketSize) {
        struct pollfd pfd{};
        pfd.fd = clientFd;
        pfd.events = POLLIN;
        int pret = poll(&pfd, 1, 50);
        if (pret <= 0) break;

        ssize_t n = read(clientFd, chunk, sizeof(chunk));
        if (n <= 0) break;
        requestBuf.append(chunk, static_cast<std::size_t>(n));

        json::Value testVal;
        if (!requestBuf.empty() && requestBuf.find('{') != std::string::npos &&
            json::Parse(requestBuf.data(), requestBuf.size(), testVal)) {
            break;
        }
    }

    if (requestBuf.size() >= hamzex::ipc::kMaxPacketSize) {
        const char* errResp = "{\"status\":\"error\",\"message\":\"Payload exceeds maximum allowed size (64KB)\"}\n";
        (void)hamzex::ipc::WriteAll(clientFd, errResp, std::strlen(errResp));
        return;
    }

    if (requestBuf.empty())
        return;

    // Strip trailing newlines/whitespace
    while (!requestBuf.empty() && (requestBuf.back() == '\n' || requestBuf.back() == '\r'))
        requestBuf.pop_back();

    json::Value req;
    if (!json::Parse(requestBuf.data(), requestBuf.size(), req) || req.type != json::Type::Object) {
        const char* errResp = "{\"status\":\"error\",\"message\":\"Invalid JSON command\"}\n";
        (void)hamzex::ipc::WriteAll(clientFd, errResp, std::strlen(errResp));
        return;
    }

    std::string cmd = json::GetString(req, "cmd");
    auto& rcfg = runtimeconfig::RuntimeConfig::Instance();

    if (cmd == "ping") {
        char resp[128];
        std::snprintf(resp, sizeof(resp),
                      "{\"status\":\"ok\",\"version\":2,\"config_version\":%zu}\n",
                      static_cast<std::size_t>(rcfg.CurrentVersion()));
        (void)hamzex::ipc::WriteAll(clientFd, resp, std::strlen(resp));
        return;
    }

    if (cmd == "get_config") {
        auto snap = rcfg.GetSnapshot();
        std::string payload = snap ? snap->rawJson : "{}";
        if (payload.empty()) payload = "{}";

        std::string resp = "{\"status\":\"ok\",\"config_version\":" +
                           std::to_string(snap ? snap->version : 0) +
                           ",\"config\":" + payload + "}\n";
        (void)hamzex::ipc::WriteAll(clientFd, resp.data(), resp.size());
        return;
    }

    if (cmd == "apply_config") {
        const json::Value* cfgVal = req.Find("config");
        std::string err;
        bool ok = false;
        if (cfgVal && cfgVal->type == json::Type::Object) {
            std::size_t configPos = requestBuf.find("\"config\"");
            std::string subJson;
            if (configPos != std::string::npos) {
                std::size_t openBrace = requestBuf.find('{', configPos);
                if (openBrace != std::string::npos) {
                    int depth = 0;
                    std::size_t endPos = openBrace;
                    for (std::size_t i = openBrace; i < requestBuf.size(); ++i) {
                        if (requestBuf[i] == '{') depth++;
                        else if (requestBuf[i] == '}') {
                            depth--;
                            if (depth == 0) {
                                endPos = i;
                                break;
                            }
                        }
                    }
                    if (depth == 0) {
                        subJson = requestBuf.substr(openBrace, endPos - openBrace + 1);
                    }
                }
            }
            if (!subJson.empty()) {
                ok = rcfg.UpdateFromJson(subJson, err);
            } else {
                ok = rcfg.UpdateFromRoot(*cfgVal, "", err);
            }
        } else {
            err = "Missing 'config' object in payload";
        }

        if (ok) {
            rcfg.SaveToFile(); // Auto-persist to ~/.config/hamzex/config.json
            char resp[128];
            std::snprintf(resp, sizeof(resp),
                          "{\"status\":\"ok\",\"config_version\":%zu}\n",
                          static_cast<std::size_t>(rcfg.CurrentVersion()));
            (void)hamzex::ipc::WriteAll(clientFd, resp, std::strlen(resp));
        } else {
            std::string resp = "{\"status\":\"error\",\"message\":\"" + err + "\"}\n";
            (void)hamzex::ipc::WriteAll(clientFd, resp.data(), resp.size());
        }
        return;
    }

    const char* unknownCmd = "{\"status\":\"error\",\"message\":\"Unknown command\"}\n";
    (void)hamzex::ipc::WriteAll(clientFd, unknownCmd, std::strlen(unknownCmd));
}
