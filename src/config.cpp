module;
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

export module Config;

import Logger;
import Json;
import ItemCatalog;

// config.json loader (~/.config/hamzex/config.json).
// Supports both modern V2 (human-readable names) and legacy V1 (settings/skins numbers).

export namespace config {

struct ChangerConfig {
    std::uint32_t ctKnifeDef = 0;      // knife def index (CT) -> Knives[] lookup
    std::uint32_t tKnifeDef = 0;       // knife def index (T)  -> Knives[] lookup
    std::uint32_t ctAgentDef = 5602;   // econ item def index (CT) -> Agents[] lookup
    std::uint32_t tAgentDef = 5602;    // econ item def index (T)  -> Agents[] lookup
    int version = 1;                   // 1 for legacy, 2 for modern
};

ChangerConfig ParseConfig(const json::Value& root);

class ConfigStore {
public:
    static ConfigStore& Instance();

    json::Value& Root() { return root_; }
    bool parseFailed() const { return parseFailed_; }

    bool isSkinEnabled() const {
        // V2: "weapons" or "knives" with skin/paint_kit
        const json::Value* weapons = root_.Find("weapons");
        if (weapons && weapons->type == json::Type::Object && !weapons->obj.empty())
            return true;
        const json::Value* knives = root_.Find("knives");
        if (knives && knives->type == json::Type::Object) {
            if (knives->Find("skin") || knives->Find("paint_kit")) return true;
            const json::Value* ct = knives->Find("ct");
            if (ct && ct->type == json::Type::Object && (ct->Find("skin") || ct->Find("paint_kit"))) return true;
            const json::Value* t = knives->Find("t");
            if (t && t->type == json::Type::Object && (t->Find("skin") || t->Find("paint_kit"))) return true;
        }
        // V1: "skins"
        const json::Value* skins = root_.Find("skins");
        return skins && skins->type == json::Type::Object && !skins->obj.empty();
    }

    bool isKnifeEnabled() const {
        if (root_.Find("knives")) return true;
        const json::Value* settings = root_.Find("settings");
        if (!settings) return false;
        return settings->Find("knife_ct_def") || settings->Find("knife_t_def");
    }

    bool isAgentEnabled() const {
        if (root_.Find("agents")) return true;
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

config::ChangerConfig config::ParseConfig(const json::Value& root) {
    ChangerConfig c;
    if (root.type != json::Type::Object)
        return c;

    long ver = json::GetLong(root, "config_version", 1);
    c.version = static_cast<int>(ver);

    static Logger log;

    // --- KNIFE RESOLUTION (V2 "knives" or V1 "settings") ---
    const json::Value* knives = root.Find("knives");
    if (knives) {
        c.version = 2;
        if (knives->type == json::Type::String) {
            const auto* km = itemcatalog::FindKnife(knives->str);
            if (km) {
                c.ctKnifeDef = km->defIndex;
                c.tKnifeDef = km->defIndex;
            } else {
                log.warn("Config: unknown knife model '" + knives->str + "'");
            }
        } else if (knives->type == json::Type::Object) {
            std::string sharedModel = json::GetStringOrNum(*knives, "model");
            if (!sharedModel.empty()) {
                const auto* km = itemcatalog::FindKnife(sharedModel);
                if (km) {
                    c.ctKnifeDef = km->defIndex;
                    c.tKnifeDef = km->defIndex;
                } else {
                    log.warn("Config: unknown knife model '" + sharedModel + "'");
                }
            }
            const json::Value* ct = knives->Find("ct");
            if (ct) {
                std::string m = (ct->type == json::Type::Object) ? json::GetStringOrNum(*ct, "model") : json::AsString(*ct);
                if (!m.empty()) {
                    const auto* km = itemcatalog::FindKnife(m);
                    if (km) c.ctKnifeDef = km->defIndex;
                    else log.warn("Config: unknown CT knife model '" + m + "'");
                }
            }
            const json::Value* t = knives->Find("t");
            if (t) {
                std::string m = (t->type == json::Type::Object) ? json::GetStringOrNum(*t, "model") : json::AsString(*t);
                if (!m.empty()) {
                    const auto* km = itemcatalog::FindKnife(m);
                    if (km) c.tKnifeDef = km->defIndex;
                    else log.warn("Config: unknown T knife model '" + m + "'");
                }
            }
        }
    } else {
        const json::Value* settings = root.Find("settings");
        if (settings) {
            c.ctKnifeDef = static_cast<std::uint32_t>(json::GetLong(*settings, "knife_ct_def", c.ctKnifeDef));
            c.tKnifeDef  = static_cast<std::uint32_t>(json::GetLong(*settings, "knife_t_def", c.tKnifeDef));
        }
    }

    // --- AGENT RESOLUTION (V2 "agents" or V1 "settings") ---
    const json::Value* agents = root.Find("agents");
    if (agents) {
        c.version = 2;
        if (agents->type == json::Type::Object) {
            std::string ctAgent = json::GetStringOrNum(*agents, "ct");
            if (!ctAgent.empty()) {
                const auto* am = itemcatalog::FindAgent(ctAgent, 3);
                if (am) c.ctAgentDef = am->def;
                else log.warn("Config: unknown CT agent '" + ctAgent + "'");
            }
            std::string tAgent = json::GetStringOrNum(*agents, "t");
            if (!tAgent.empty()) {
                const auto* am = itemcatalog::FindAgent(tAgent, 2);
                if (am) c.tAgentDef = am->def;
                else log.warn("Config: unknown T agent '" + tAgent + "'");
            }
        }
    } else {
        const json::Value* settings = root.Find("settings");
        if (settings) {
            c.ctAgentDef = static_cast<std::uint32_t>(json::GetLong(*settings, "agent_ct_def", c.ctAgentDef));
            c.tAgentDef  = static_cast<std::uint32_t>(json::GetLong(*settings, "agent_t_def", c.tAgentDef));
        }
    }

    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "Config: loaded v%d (ctKnife=%u, tKnife=%u, ctAgent=%u, tAgent=%u)",
                  c.version, c.ctKnifeDef, c.tKnifeDef, c.ctAgentDef, c.tAgentDef);
    log.info(buf);
    return c;
}

config::ChangerConfig& config::Global() {
    static ChangerConfig cfg = [] {
        ConfigStore& store = ConfigStore::Instance();
        if (store.parseFailed()) {
            static Logger log;
            log.warn("config: parse failed, using defaults");
            return ChangerConfig{};
        }
        return ParseConfig(store.Root());
    }();
    return cfg;
}