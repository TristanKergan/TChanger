module;
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <unordered_map>

export module SkinConfig;

import Logger;
import Json;
import Config;

// config.json "skins" bölümünden silah bazlı skin tablosu (Kernel-Cheat formatı):
// "both" / "ct" / "t" altında silah adı -> { paint_kit, seed, wear }.
// Arama önceliği: takım (ct=3 / t=2), yoksa "both".

export namespace skinconfig {

struct SkinEntry {
    std::int32_t paint_kit = 38;
    std::int32_t seed = 0;
    float wear = 0.0001f;
};

class SkinTable {
public:
    using Map = std::unordered_map<std::string, SkinEntry>;

    bool Load(const json::Value& skins);
    bool Get(std::string_view weapon, std::uint8_t team, SkinEntry& out) const;
    std::size_t size() const noexcept { return both_.size() + ct_.size() + t_.size(); }
    static SkinTable& Global();

private:
    Map both_;
    Map ct_;
    Map t_;
};

}  // namespace skinconfig

namespace {

void ParseSection(const json::Value& section, skinconfig::SkinTable::Map& dst) {
    if (section.type != json::Type::Object)
        return;
    for (const auto& kv : section.obj) {
        const json::Value& entry = kv.second;
        if (entry.type != json::Type::Object)
            continue;
        skinconfig::SkinEntry se;
        se.paint_kit = static_cast<std::int32_t>(json::GetLong(entry, "paint_kit", se.paint_kit));
        se.seed = static_cast<std::int32_t>(json::GetLong(entry, "seed", se.seed));
        se.wear = static_cast<float>(json::GetDouble(entry, "wear", se.wear));
        dst.emplace(kv.first, se);
    }
}

}  // namespace

bool skinconfig::SkinTable::Load(const json::Value& skins) {
    both_.clear();
    ct_.clear();
    t_.clear();
    if (skins.type != json::Type::Object)
        return false;
    for (const auto& kv : skins.obj) {
        if (kv.first == "both")
            ParseSection(kv.second, both_);
        else if (kv.first == "ct")
            ParseSection(kv.second, ct_);
        else if (kv.first == "t")
            ParseSection(kv.second, t_);
    }
    return true;
}

bool skinconfig::SkinTable::Get(std::string_view weapon, std::uint8_t team, SkinEntry& out) const {
    const std::string key(weapon);
    if (team == 3) {
        const auto it = ct_.find(key);
        if (it != ct_.end()) {
            out = it->second;
            return true;
        }
    } else if (team == 2) {
        const auto it = t_.find(key);
        if (it != t_.end()) {
            out = it->second;
            return true;
        }
    }
    const auto it = both_.find(key);
    if (it != both_.end()) {
        out = it->second;
        return true;
    }
    return false;
}

skinconfig::SkinTable& skinconfig::SkinTable::Global() {
    static SkinTable table = [] {
        SkinTable t;
        const json::Value* skins = config::ConfigStore::Instance().Root().Find("skins");
        if (skins && t.Load(*skins)) {
            static Logger log;
            char buf[128];
            std::snprintf(buf, sizeof(buf), "SkinTable: loaded %zu weapons from config.json",
                          t.size());
            log.info(buf);
        } else {
            static Logger log;
            log.warn("SkinTable: no 'skins' section (no skins applied)");
        }
        return t;
    }();
    return table;
}
