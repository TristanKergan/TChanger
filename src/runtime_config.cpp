module;
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

export module RuntimeConfig;

import Logger;
import Json;
import ItemCatalog;
import Config;
import SkinConfig;

export namespace runtimeconfig {

struct RuntimeConfigSnapshot {
    std::uint64_t version = 1;
    config::ChangerConfig changer;
    skinconfig::SkinTable skins;
    std::string rawJson;
};

class RuntimeConfig {
public:
    static RuntimeConfig& Instance();

    // Fast, lock-free acquire for game rendering / FSN hook thread.
    // Zero mutex locks, zero wait time, no data races.
    std::shared_ptr<const RuntimeConfigSnapshot> GetSnapshot() const noexcept {
        return snapshot_.load(std::memory_order_acquire);
    }

    bool UpdateFromRoot(const json::Value& root, std::string_view rawJson, std::string& outError);
    bool UpdateFromJson(std::string_view jsonStr, std::string& outError);
    bool LoadFromFile(const std::string& path = "");
    bool SaveToFile(const std::string& path = "") const;

    std::uint64_t CurrentVersion() const noexcept {
        auto snap = GetSnapshot();
        return snap ? snap->version : 0;
    }

private:
    RuntimeConfig();
    std::atomic<std::shared_ptr<const RuntimeConfigSnapshot>> snapshot_;
    mutable std::mutex updateMutex_;
};

}  // namespace runtimeconfig

namespace {

std::string ResolveConfigPath(const std::string& overridePath) {
    if (!overridePath.empty())
        return overridePath;
    const char* home = std::getenv("HOME");
    std::string base = (home && *home) ? std::string(home) : std::string(".");
    const std::string canonical = base + "/.config/hamzex/config.json";
    std::FILE* f = std::fopen(canonical.c_str(), "rb");
    if (f) {
        std::fclose(f);
        return canonical;
    }
    const std::string fallback = base + "/.config/Hamzex/config.json";
    f = std::fopen(fallback.c_str(), "rb");
    if (f) {
        std::fclose(f);
        return fallback;
    }
    return canonical;
}

}  // namespace

runtimeconfig::RuntimeConfig& runtimeconfig::RuntimeConfig::Instance() {
    static RuntimeConfig inst;
    return inst;
}

runtimeconfig::RuntimeConfig::RuntimeConfig() {
    auto initial = std::make_shared<RuntimeConfigSnapshot>();
    initial->version = 1;
    snapshot_.store(initial, std::memory_order_release);
    LoadFromFile();
}

bool runtimeconfig::RuntimeConfig::UpdateFromRoot(const json::Value& root,
                                                  std::string_view rawJson,
                                                  std::string& outError) {
    if (root.type != json::Type::Object) {
        outError = "Root JSON must be an object";
        return false;
    }

    std::lock_guard<std::mutex> lock(updateMutex_);

    auto prev = snapshot_.load(std::memory_order_relaxed);
    auto next = std::make_shared<RuntimeConfigSnapshot>();
    next->version = prev ? (prev->version + 1) : 1;
    next->changer = config::ParseConfig(root);

    if (!next->skins.Load(root)) {
        static Logger log;
        log.warn("RuntimeConfig: no weapons/skins loaded from new config");
    }

    next->rawJson = rawJson.empty() ? (prev ? prev->rawJson : "") : std::string(rawJson);

    snapshot_.store(next, std::memory_order_release);

    static Logger log;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "RuntimeConfig: snapshot v%zu published (ctKnife=%u, tKnife=%u)",
                  static_cast<std::size_t>(next->version),
                  next->changer.ctKnifeDef, next->changer.tKnifeDef);
    log.info(buf);
    return true;
}

bool runtimeconfig::RuntimeConfig::UpdateFromJson(std::string_view jsonStr, std::string& outError) {
    if (jsonStr.empty()) {
        outError = "Empty JSON payload";
        return false;
    }

    json::Value root;
    if (!json::Parse(jsonStr.data(), jsonStr.size(), root)) {
        outError = "Failed to parse JSON: invalid syntax";
        return false;
    }

    return UpdateFromRoot(root, jsonStr, outError);
}

bool runtimeconfig::RuntimeConfig::LoadFromFile(const std::string& path) {
    const std::string targetPath = ResolveConfigPath(path);
    std::FILE* f = std::fopen(targetPath.c_str(), "rb");
    if (!f)
        return false;

    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return false;
    }
    const long sz = std::ftell(f);
    if (sz < 0 || std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        return false;
    }
    std::string buf(static_cast<std::size_t>(sz), '\0');
    const bool ok = (sz == 0) || (std::fread(&buf[0], 1, static_cast<std::size_t>(sz), f) ==
                                  static_cast<std::size_t>(sz));
    std::fclose(f);
    if (!ok || sz == 0)
        return false;

    std::string err;
    return UpdateFromJson(buf, err);
}

bool runtimeconfig::RuntimeConfig::SaveToFile(const std::string& path) const {
    const std::string targetPath = ResolveConfigPath(path);
    auto snap = GetSnapshot();
    if (!snap || snap->rawJson.empty())
        return false;

    // Ensure directory exists using POSIX mkdir (no shell invocation)
    std::size_t lastSlash = targetPath.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string dir = targetPath.substr(0, lastSlash);
        std::size_t pos = 1;
        while ((pos = dir.find('/', pos)) != std::string::npos) {
            std::string sub = dir.substr(0, pos);
            (void)mkdir(sub.c_str(), 0700);
            ++pos;
        }
        (void)mkdir(dir.c_str(), 0700);
    }

    std::string bakPath = targetPath + ".bak";
    bool hadExisting = (access(targetPath.c_str(), F_OK) == 0);
    if (hadExisting) {
        unlink(bakPath.c_str());
        (void)std::rename(targetPath.c_str(), bakPath.c_str());
    }

    std::FILE* f = std::fopen(targetPath.c_str(), "wb");
    if (!f) {
        if (hadExisting) {
            (void)std::rename(bakPath.c_str(), targetPath.c_str());
        }
        return false;
    }

    std::size_t written = std::fwrite(snap->rawJson.data(), 1, snap->rawJson.size(), f);
    std::fclose(f);
    return written == snap->rawJson.size();
}
