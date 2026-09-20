module;
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

export module SchemaScan;

import Logger;
import Modules;
import Memory;

// Runtime şema taraması (legacy skin_schema.cpp portu). libschemasystem.so ve
// libclient.so exec segmentlerinde şema sistemi singleton'ını bulur, scope
// hash tablosu + free list'i gezerek sınıf/alan offset'lerini çözer. /proc/self/mem
// yerine IsBadReadPtr ile korunan doğrudan okuma kullanılır (bogus pointer crash'e yol açmaz).

namespace {

constexpr const char* kSchemaSystemModule = "libschemasystem.so";

constexpr int kMaxScopes = 100;
constexpr int kHashBuckets = 1024;
constexpr int kHashWalkLimit = 256;
constexpr int kFreeListLimit = 2048;
constexpr std::size_t kClassCapacity = 8192;
constexpr std::size_t kClassNameCap = 64;
constexpr std::size_t kFieldNameCap = 64;
constexpr std::size_t kScopeNameCap = 128;

// SchemaSystem pointer patterns (libschemasystem.so exec segmentlerinde).
const char* kSchemaSystemPatterns[] = {
    "48 8B 05 ? ? ? ? 48 8D 15 ? ? ? ? 4C 89 E9",
    "48 89 05 ? ? ? ? 48 8D 0D ? ? ? ? E9",
    nullptr,
};

struct SchemaFieldEntry {
    std::string name;
    int offset = 0;
};
struct SchemaClassInfo {
    std::string name;
    int size = 0;
    std::vector<SchemaFieldEntry> fields;
};
struct Schema {
    std::vector<SchemaClassInfo> classes;
};

const SchemaClassInfo* SchemaFindClass(const Schema& schema, const std::string& name) {
    for (const auto& cls : schema.classes)
        if (cls.name == name)
            return &cls;
    return nullptr;
}
const SchemaFieldEntry* SchemaFindField(const SchemaClassInfo& cls, const std::string& name) {
    for (const auto& f : cls.fields)
        if (f.name == name)
            return &f;
    return nullptr;
}

bool ParseClassNode(Schema& out, std::uintptr_t data, bool skipExisting) {
    if (!data || out.classes.size() >= kClassCapacity)
        return false;
    std::uintptr_t cnPtr = 0;
    if (!ReadPtr(data + 0x08, &cnPtr) || !cnPtr)
        return false;
    char clsName[kClassNameCap] = {0};
    if (!ReadString(cnPtr, clsName, sizeof(clsName)) || !clsName[0])
        return false;
    if (skipExisting && SchemaFindClass(out, clsName))
        return false;
    std::int16_t fcount = 0;
    if (!ReadI16(data + 0x24, &fcount))
        return false;
    std::int32_t clsSize = 0;
    std::uintptr_t fv = 0;
    ReadI32(data + 0x20, &clsSize);
    ReadPtr(data + 0x30, &fv);
    SchemaClassInfo ci;
    ci.name = clsName;
    ci.size = clsSize;
    if (fv && fcount >= 0 && fcount <= 20000) {
        ci.fields.reserve(static_cast<std::size_t>(fcount));
        for (int fi = 0; fi < fcount; ++fi) {
            const std::uintptr_t fa = fv + static_cast<std::uintptr_t>(fi) * 0x20;
            std::uintptr_t fnPtr = 0;
            if (!ReadPtr(fa, &fnPtr) || !fnPtr)
                continue;
            char fname[kFieldNameCap] = {0};
            if (!ReadString(fnPtr, fname, sizeof(fname)) || !fname[0])
                continue;
            std::int32_t fo = 0;
            if (!ReadI32(fa + 0x10, &fo))
                continue;
            SchemaFieldEntry fe;
            fe.name = fname;
            fe.offset = fo;
            ci.fields.push_back(std::move(fe));
        }
    }
    out.classes.push_back(std::move(ci));
    return true;
}

int ParseSchema(Schema& out, std::uintptr_t system) {
    std::int32_t numScopes = 0;
    std::uintptr_t scopesVec = 0;
    if (!ReadI32(system + 0x1F0, &numScopes))
        return -1;
    if (!ReadPtr(system + 0x1F8, &scopesVec))
        return -1;
    if (numScopes < 0)
        numScopes = 0;
    if (numScopes > kMaxScopes)
        numScopes = kMaxScopes;
    for (std::int32_t i = 0; i < numScopes; ++i) {
        std::uintptr_t scopeAddr = 0;
        if (!ReadPtr(scopesVec + static_cast<std::uintptr_t>(i) * 8, &scopeAddr) || !scopeAddr)
            continue;
        char scopeName[kScopeNameCap] = {0};
        if (!ReadString(scopeAddr + 0x08, scopeName, sizeof(scopeName)))
            continue;
        if (std::strstr(scopeName, "libclient.so") == nullptr)
            continue;
        const std::uintptr_t hashVec = scopeAddr + 0x560 + 0x90;
        for (int b = 0; b < kHashBuckets; ++b) {
            std::uintptr_t cur = 0;
            if (!ReadPtr(hashVec + static_cast<std::uintptr_t>(b) * 24 + 0x28, &cur))
                continue;
            int loopSafety = 0;
            while (cur && loopSafety++ < kHashWalkLimit) {
                std::uintptr_t data = 0;
                if (!ReadPtr(cur + 0x10, &data))
                    break;
                ParseClassNode(out, data, false);
                std::uintptr_t next = 0;
                if (!ReadPtr(cur + 8, &next))
                    break;
                cur = next;
            }
        }
        std::uintptr_t blob = 0;
        if (!ReadPtr(scopeAddr + 0x560 + 0x20, &blob))
            continue;
        int listSafety = 0;
        while (blob && listSafety++ < kFreeListLimit) {
            std::uintptr_t data = 0;
            if (!ReadPtr(blob + 0x10, &data))
                break;
            ParseClassNode(out, data, true);
            std::uintptr_t next = 0;
            if (!ReadPtr(blob, &next))
                break;
            blob = next;
        }
    }
    return 0;
}

bool ResolveSchemaModule(const std::string& moduleName, Schema& out) {
    const auto segments = FindExecutableSegments(moduleName);
    if (segments.empty())
        return false;
    std::uintptr_t found = 0;
    for (int pi = 0; kSchemaSystemPatterns[pi]; ++pi) {
        const std::uintptr_t m = PatternScan(moduleName, kSchemaSystemPatterns[pi]);
        if (m) {
            found = m;
            break;
        }
    }
    if (!found)
        return false;
    const std::uintptr_t sysPtr = ResolveRipRel(found + 3);
    if (!sysPtr)
        return false;
    std::uintptr_t system = 0;
    if (!ReadPtr(sysPtr, &system) || !system)
        return false;
    return ParseSchema(out, system) == 0;
}

void SetOffset(const Schema& s, const char* cls, const char* field, int* target) {
    if (const auto* c = SchemaFindClass(s, cls))
        if (const auto* f = SchemaFindField(*c, field))
            *target = f->offset;
}

}  // namespace

export struct SkinSchemaOffsets {
    int weapon_services_offset = 0;      // C_BasePlayerPawn::m_pWeaponServices
    int my_weapons_offset = 0;           // CPlayer_WeaponServices::m_hMyWeapons
    int attribute_manager_offset = 0;     // C_EconEntity::m_AttributeManager
    int item_in_container_offset = 0;     // C_AttributeContainer::m_Item
    int item_id_high_offset = 0;          // C_EconItemView::m_iItemIDHigh
    int item_id_low_offset = 0;           // C_EconItemView::m_iItemIDLow
    int item_def_index_offset = 0;        // C_EconItemView::m_iItemDefinitionIndex
    int item_quality_offset = 0;          // C_EconItemView::m_iEntityQuality
    int account_id_offset = 0;            // C_EconItemView::m_iAccountID
    int attribute_list_offset = 0;        // C_EconItemView::m_AttributeList
    int attributes_offset = 0;            // CAttributeList::m_Attributes
    int fallback_paint_kit_offset = 0;    // C_EconEntity::m_nFallbackPaintKit
    int fallback_seed_offset = 0;         // C_EconEntity::m_nFallbackSeed
    int fallback_wear_offset = 0;         // C_EconEntity::m_flFallbackWear
    int fallback_stat_trak_offset = 0;    // C_EconEntity::m_nFallbackStatTrak
    int subclass_id_offset = 0;           // C_BaseEntity::m_nSubclassID
    int team_num_offset = 0;               // C_BaseEntity/C_BasePlayerPawn::m_iTeamNum

    bool Ready() const {
        return attribute_manager_offset && item_in_container_offset &&
               item_def_index_offset && item_id_high_offset && item_id_low_offset &&
               item_quality_offset && account_id_offset && attribute_list_offset &&
               attributes_offset && fallback_paint_kit_offset && fallback_seed_offset &&
               fallback_wear_offset;
    }
};

export SkinSchemaOffsets& ResolveSkin() {
    static SkinSchemaOffsets out;
    static bool done = false;
    static long calls = 0;
    static int scanAttempts = 0;
    static long nextAttemptCall = 0;

    if (done)
        return out;

    if (!out.Ready()) {
        if (calls < nextAttemptCall) {
            calls++;
            return out;
        }
        if (scanAttempts >= 5) {
            calls++;
            return out;
        }
        scanAttempts++;
        // Backoff: 120, 240, 480, 960 frames
        nextAttemptCall = calls + 120 * (1 << (scanAttempts - 1));
        calls++;
    }
    static Logger log;
    out = SkinSchemaOffsets{};

    // Primary: libschemasystem.so
    {
        Schema schema;
        if (!ResolveSchemaModule(kSchemaSystemModule, schema))
            log.error("SkinSchema: libschemasystem.so scan failed");
        else {
            SetOffset(schema, "C_BasePlayerPawn", "m_pWeaponServices", &out.weapon_services_offset);
            SetOffset(schema, "CPlayer_WeaponServices", "m_hMyWeapons", &out.my_weapons_offset);
            SetOffset(schema, "CCSPlayer_WeaponServices", "m_hMyWeapons", &out.my_weapons_offset);
            SetOffset(schema, "C_EconEntity", "m_AttributeManager", &out.attribute_manager_offset);
            SetOffset(schema, "C_EconEntity", "m_nFallbackPaintKit", &out.fallback_paint_kit_offset);
            SetOffset(schema, "C_EconEntity", "m_nFallbackSeed", &out.fallback_seed_offset);
            SetOffset(schema, "C_EconEntity", "m_flFallbackWear", &out.fallback_wear_offset);
            SetOffset(schema, "C_EconEntity", "m_nFallbackStatTrak", &out.fallback_stat_trak_offset);
            SetOffset(schema, "C_AttributeContainer", "m_Item", &out.item_in_container_offset);
            SetOffset(schema, "C_EconItemView", "m_iItemDefinitionIndex", &out.item_def_index_offset);
            SetOffset(schema, "C_EconItemView", "m_iItemIDHigh", &out.item_id_high_offset);
            SetOffset(schema, "C_EconItemView", "m_iItemIDLow", &out.item_id_low_offset);
            SetOffset(schema, "C_EconItemView", "m_iEntityQuality", &out.item_quality_offset);
            SetOffset(schema, "C_EconItemView", "m_iAccountID", &out.account_id_offset);
            SetOffset(schema, "C_EconItemView", "m_AttributeList", &out.attribute_list_offset);
            SetOffset(schema, "CAttributeList", "m_Attributes", &out.attributes_offset);
            SetOffset(schema, "C_BaseEntity", "m_nSubclassID", &out.subclass_id_offset);
            SetOffset(schema, "C_BaseEntity", "m_iTeamNum", &out.team_num_offset);
            SetOffset(schema, "C_BasePlayerPawn", "m_iTeamNum", &out.team_num_offset);
        }
    }

    char buf[320];
    std::snprintf(buf, sizeof(buf),
                  "SkinSchema offsets: wpn_svc=0x%X my_wpn=0x%X attr=0x%X paint=0x%X seed=0x%X "
                  "wear=0x%X stat=0x%X item=0x%X id_high=0x%X id_low=0x%X def_idx=0x%X "
                  "quality=0x%X account=0x%X attr_list=0x%X attrs=0x%X subclass=0x%X team_num=0x%X",
                  out.weapon_services_offset, out.my_weapons_offset, out.attribute_manager_offset,
                  out.fallback_paint_kit_offset, out.fallback_seed_offset, out.fallback_wear_offset,
                  out.fallback_stat_trak_offset, out.item_in_container_offset, out.item_id_high_offset,
                  out.item_id_low_offset, out.item_def_index_offset, out.item_quality_offset,
                  out.account_id_offset, out.attribute_list_offset, out.attributes_offset,
                  out.subclass_id_offset, out.team_num_offset);
    if (out.Ready()) {
        done = true;
        log.info(buf);
    } else {
        log.error(buf);
    }
    return out;
}
