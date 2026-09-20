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
import ItemCatalog;

// config.json skin tablosu yükleyici:
// V2 ("weapons" / "knives") ve V1 ("skins" altinda "both" / "ct" / "t") formatlarını destekler.
// Silah isimlerini (ak47, m4a1_s, usp_s vb.) ve skin isimlerini (fade, head_shot, printstream vb.)
// otomatik çözer ve doğrular.

export namespace skinconfig {

struct SkinEntry {
    std::int32_t paint_kit = 38;
    std::int32_t seed = 1;
    float wear = 0.0001f;
};

struct StringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
    std::size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

class SkinTable {
public:
    using Map = std::unordered_map<std::string, SkinEntry, StringHash, std::equal_to<>>;

    bool Load(const json::Value& root);
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

skinconfig::SkinEntry ParseSkinObject(const json::Value& entry, std::string_view weapon) {
    skinconfig::SkinEntry se;
    // 1. Skin name or paint_kit ID
    std::string skinStr = json::GetStringOrNum(entry, "skin");
    if (skinStr.empty())
        skinStr = json::GetStringOrNum(entry, "paint_kit");
    if (!skinStr.empty()) {
        se.paint_kit = itemcatalog::ResolvePaintKit(weapon, skinStr, se.paint_kit);
    }

    // 2. Wear (number or string preset: fn, mw, ft, ww, bs, factory_new, etc.)
    std::string wearStr = json::GetStringOrNum(entry, "wear");
    if (!wearStr.empty()) {
        se.wear = itemcatalog::ResolveWear(wearStr, se.wear);
    }

    // 3. Seed (number or string)
    std::string seedStr = json::GetStringOrNum(entry, "seed");
    if (!seedStr.empty()) {
        se.seed = itemcatalog::ResolveSeed(seedStr, se.seed);
    }
    return se;
}

void InsertEntry(skinconfig::SkinTable::Map& dst, std::string_view rawName, const skinconfig::SkinEntry& se) {
    dst[std::string(rawName)] = se;
    const char* canon = itemcatalog::CanonicalWeaponName(rawName);
    if (canon && rawName != canon) {
        dst[std::string(canon)] = se;
    }
}

void ParseV1Section(const json::Value& section, skinconfig::SkinTable::Map& dst) {
    if (section.type != json::Type::Object)
        return;
    for (const auto& kv : section.obj) {
        const json::Value& entry = kv.second;
        if (entry.type != json::Type::Object)
            continue;
        skinconfig::SkinEntry se = ParseSkinObject(entry, kv.first);
        InsertEntry(dst, kv.first, se);
    }
}

}  // namespace

bool skinconfig::SkinTable::Load(const json::Value& root) {
    both_.clear();
    ct_.clear();
    t_.clear();

    if (root.type != json::Type::Object)
        return false;

    static Logger log;
    bool loadedAny = false;

    // --- 1. Modern V2: "weapons" ---
    const json::Value* weapons = root.Find("weapons");
    if (weapons && weapons->type == json::Type::Object) {
        for (const auto& kv : weapons->obj) {
            const std::string& rawName = kv.first;
            const json::Value& entryVal = kv.second;
            if (entryVal.type != json::Type::Object)
                continue;

            // Check "enabled": false
            const json::Value* en = entryVal.Find("enabled");
            if (en && en->type == json::Type::Bool && !en->b)
                continue;

            const char* canon = itemcatalog::CanonicalWeaponName(rawName);
            const std::string_view weaponKey = canon ? canon : rawName;

            // Team-split weapon: e.g. "deagle": { "ct": {...}, "t": {...} }
            const json::Value* ctSub = entryVal.Find("ct");
            const json::Value* tSub = entryVal.Find("t");
            if (ctSub || tSub) {
                if (ctSub && ctSub->type == json::Type::Object) {
                    SkinEntry se = ParseSkinObject(*ctSub, weaponKey);
                    InsertEntry(ct_, rawName, se);
                    loadedAny = true;
                }
                if (tSub && tSub->type == json::Type::Object) {
                    SkinEntry se = ParseSkinObject(*tSub, weaponKey);
                    InsertEntry(t_, rawName, se);
                    loadedAny = true;
                }
                continue;
            }

            SkinEntry se = ParseSkinObject(entryVal, weaponKey);
            InsertEntry(both_, rawName, se);
            loadedAny = true;
        }
    }

    // --- 2. Modern V2: "knives" with skin ---
    const json::Value* knives = root.Find("knives");
    if (knives && knives->type == json::Type::Object) {
        // Shared knife skin
        if (knives->Find("skin") || knives->Find("paint_kit")) {
            SkinEntry se = ParseSkinObject(*knives, "knife");
            both_["knife"] = se;
            loadedAny = true;
        }
        // CT knife skin
        const json::Value* ct = knives->Find("ct");
        if (ct && ct->type == json::Type::Object) {
            if (ct->Find("skin") || ct->Find("paint_kit")) {
                SkinEntry se = ParseSkinObject(*ct, "knife");
                ct_["knife"] = se;
                std::string m = json::GetStringOrNum(*ct, "model");
                if (!m.empty()) ct_[m] = se;
                loadedAny = true;
            }
        }
        // T knife skin
        const json::Value* t = knives->Find("t");
        if (t && t->type == json::Type::Object) {
            if (t->Find("skin") || t->Find("paint_kit")) {
                SkinEntry se = ParseSkinObject(*t, "knife");
                t_["knife"] = se;
                std::string m = json::GetStringOrNum(*t, "model");
                if (!m.empty()) t_[m] = se;
                loadedAny = true;
            }
        }
    }

    // --- 3. Legacy V1: "skins" (both / ct / t) or root itself as skins object ---
    const json::Value* skins = root.Find("skins");
    const json::Value& skinsSec = skins ? *skins : root;
    if (skinsSec.type == json::Type::Object) {
        const json::Value* b = skinsSec.Find("both");
        if (b) { ParseV1Section(*b, both_); loadedAny = true; }
        const json::Value* c = skinsSec.Find("ct");
        if (c) { ParseV1Section(*c, ct_); loadedAny = true; }
        const json::Value* tSec = skinsSec.Find("t");
        if (tSec) { ParseV1Section(*tSec, t_); loadedAny = true; }
    }

    return loadedAny;
}

bool skinconfig::SkinTable::Get(std::string_view weapon, std::uint8_t team, SkinEntry& out) const {
    const char* canon = itemcatalog::CanonicalWeaponName(weapon);
    std::string_view canonSv = canon ? canon : weapon;

    if (team == 3) {
        auto it = ct_.find(weapon);
        if (it != ct_.end()) { out = it->second; return true; }
        if (canon) {
            it = ct_.find(canonSv);
            if (it != ct_.end()) { out = it->second; return true; }
        }
    } else if (team == 2) {
        auto it = t_.find(weapon);
        if (it != t_.end()) { out = it->second; return true; }
        if (canon) {
            it = t_.find(canonSv);
            if (it != t_.end()) { out = it->second; return true; }
        }
    }

    auto it = both_.find(weapon);
    if (it != both_.end()) { out = it->second; return true; }
    if (canon) {
        it = both_.find(canonSv);
        if (it != both_.end()) { out = it->second; return true; }
    }

    return false;
}

skinconfig::SkinTable& skinconfig::SkinTable::Global() {
    static SkinTable table = [] {
        SkinTable t;
        const json::Value& root = config::ConfigStore::Instance().Root();
        if (t.Load(root)) {
            static Logger log;
            char buf[128];
            std::snprintf(buf, sizeof(buf), "SkinTable: loaded %zu weapon entries from config.json",
                          t.size());
            log.info(buf);
        } else {
            static Logger log;
            log.warn("SkinTable: no weapons/skins configured (no skins applied)");
        }
        return t;
    }();
    return table;
}
