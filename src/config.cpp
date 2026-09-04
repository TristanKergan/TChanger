module;
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

export module Config;

import Logger;
import Json;

// config.json yükleyici (~/.config/parrot/config.json). Sadece OKUMA: ayarları
// ("settings") ayrıştırır; skin tablosu SkinConfig modülü tarafından ayrıca okunur.
// Harici bağımlılık yok, exception'suz. OBF kullanılmaz (düz metin).

export namespace config {

struct ChangerConfig {
    int ctKnifeIndex = 0;          // kKnives[] index (CT)
    int tKnifeIndex = 0;           // kKnives[] index (T)
    std::uint32_t ctAgentDef = 5602;  // econ item def index (CT) -> Agents[] lookup
    std::uint32_t tAgentDef = 5602;   // econ item def index (T)  -> Agents[] lookup
};

class ConfigStore {
public:
    static ConfigStore& Instance();

    json::Value& Root() { return root_; }
    bool parseFailed() const { return parseFailed_; }

    bool isSkinEnabled() const {
        const json::Value* skins = root_.Find("skins");
        return skins && skins->type == json::Type::Object && !skins->obj.empty();
    }
    bool isKnifeEnabled() const {
        const json::Value* settings = root_.Find("settings");
        if (!settings) return false;
        return settings->Find("knife_ct_index") || settings->Find("knife_t_index");
    }
    bool isAgentEnabled() const {
        const json::Value* settings = root_.Find("settings");
        if (!settings) return false;
        return settings->Find("agent_ct_def") || settings->Find("agent_t_def");
    }

private:
    ConfigStore();
    json::Value root_;
    bool parseFailed_ = false;
};

// Process-wide config; ilk çağrıda dosyayı okur. Dosya eksik/bozuksa varsayılanlar.
ChangerConfig& Global();

}  // namespace config

namespace {

std::string ConfigPath() {
    const char* home = std::getenv("HOME");
    std::string base = (home && *home) ? std::string(home) : std::string(".");
    return base + "/.config/hamzex/config.json";
}

}  // namespace

config::ConfigStore& config::ConfigStore::Instance() {
    static ConfigStore store;
    return store;
}

config::ConfigStore::ConfigStore() {
    root_.type = json::Type::Object;
    const std::string path = ConfigPath();
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return;  // eksik -> varsayılanlar
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return;
    }
    const long sz = std::ftell(f);
    if (sz < 0 || std::fseek(f, 0, SEEK_SET) != 0) {
        std::fclose(f);
        return;
    }
    std::string buf(static_cast<std::size_t>(sz), '\0');
    const bool ok = (sz == 0) || (std::fread(&buf[0], 1, static_cast<std::size_t>(sz), f) ==
                                  static_cast<std::size_t>(sz));
    std::fclose(f);
    if (!ok) {
        static Logger log;
        log.error("config: read failed");
        parseFailed_ = true;
        return;
    }
    if (sz == 0)
        return;  // boş dosya -> varsayılanlar

    json::Value root;
    if (!json::Parse(buf.data(), buf.size(), root) || root.type != json::Type::Object) {
        static Logger log;
        log.error("config: not a valid JSON object");
        parseFailed_ = true;
        return;
    }
    root_ = std::move(root);
}

config::ChangerConfig& config::Global() {
    static ChangerConfig cfg = [] {
        ChangerConfig c;
        ConfigStore& store = ConfigStore::Instance();
        if (store.parseFailed()) {
            static Logger log;
            log.warn("config: parse failed, using defaults");
            return c;
        }
        const json::Value* settings = store.Root().Find("settings");
        if (!settings) {
            static Logger log;
            log.info("config: no 'settings' section, using defaults");
            return c;
        }
        c.ctKnifeIndex = static_cast<int>(json::GetLong(*settings, "knife_ct_index", c.ctKnifeIndex));
        c.tKnifeIndex  = static_cast<int>(json::GetLong(*settings, "knife_t_index", c.tKnifeIndex));
        c.ctAgentDef = static_cast<std::uint32_t>(json::GetLong(*settings, "agent_ct_def", c.ctAgentDef));
        c.tAgentDef  = static_cast<std::uint32_t>(json::GetLong(*settings, "agent_t_def", c.tAgentDef));
        return c;
    }();
    return cfg;
}
