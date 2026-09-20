module;
#include <cstdint>
#include <cstdio>

export module SkinChanger;

import Logger;
import Modules;
import Memory;
import Resolver;
import SchemaScan;
import AttributeManager;
import Offsets;
import Config;
import SkinConfig;
import ItemCatalog;

// Skin + knife changer. Called from the FSN hook:
//   Run()        -> stage 6 (FRAME_NET_UPDATE_POSTDATAUPDATE_START): per-weapon skin
//                   from config.json ("skins" tablosu, takım farkında) + default
//                   bıçak model swap (config'ten seçilen bıçak). SetModel ertelenir.
//   RunSetModels()-> stage 7 (FRAME_NET_UPDATE_POSTDATAUPDATE_END): ertelenen bıçak
//                   SetModel'ını uygular (stage-6 SetModel ölüm teardown'ında crash).
//
// Econ field offset'leri SchemaScan modülünden, client pointer/fonksiyonları
// Resolver'dan, ayarlar/skin tablosu Config/SkinConfig modüllerinden gelir.

namespace {

constexpr std::uint32_t kDefaultKnifeDefIndex42 = 42;
constexpr std::uint32_t kDefaultKnifeDefIndex59 = 59;
constexpr std::uint32_t kAccountId = 0x1337BEEF;
constexpr std::int32_t kQuality = 3;

constexpr int kMaxWeapons = 64;

// Weapon item definition index -> canonical name
const char* weapon_name_from_index(int index) {
    const auto* w = itemcatalog::FindWeaponByDef(static_cast<std::uint32_t>(index));
    if (w) return w->canonicalName;
    if (itemcatalog::IsKnifeDefIndex(static_cast<std::uint32_t>(index))) return "knife";
    return nullptr;
}

}  // namespace

export class SkinChanger {
public:
    static SkinChanger& Global();
    void Run();          // stage 6
    void RunSetModels(); // stage 7

private:
    std::uint32_t pendingKnifeHandle_ = 0;
    const char* pendingSetModelPath_ = nullptr;
};

SkinChanger& SkinChanger::Global() {
    static SkinChanger instance;
    return instance;
}

void SkinChanger::Run() {
    static Logger log;
    static bool dResolve = false, dSchema = false, dCtrl = false,
                dSys = false, dPawn = false, dWps = false, dTeam = false;

    const auto& o = ResolveClientOffsets();
    if (!o.resolved || !o.localPlayerController || !o.entitySystem || !o.setModel) {
        if (!dResolve) { log.error("SkinChanger: client offsets not resolved"); dResolve = true; }
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }

    const auto& so = ResolveSkin();
    if (!so.Ready()) {
        if (!dSchema) { log.error("SkinChanger: schema offsets incomplete"); dSchema = true; }
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }

    if (so.team_num_offset == 0) {
        if (!dTeam) { log.error("SkinChanger: m_iTeamNum offset unresolved"); dTeam = true; }
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }

    void* controller = TryController(o.localPlayerController, ClientModule);
    if (!controller || IsBadReadPtr(reinterpret_cast<const void*>(controller), 8)) {
        if (!dCtrl) { log.warn("SkinChanger: controller null/bad"); dCtrl = true; }
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }

    if (IsBadReadPtr(reinterpret_cast<const std::uint8_t*>(controller) + o.m_hPawn, sizeof(std::uint32_t))) {
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }
    const auto pawnHandle = *reinterpret_cast<const std::uint32_t*>(
        reinterpret_cast<const std::uint8_t*>(controller) + o.m_hPawn);

    const auto system = ReadPointer(o.entitySystem);
    if (!system || IsBadReadPtr(reinterpret_cast<const void*>(system), sizeof(void*))) {
        if (!dSys) { log.warn("SkinChanger: entitySystem null/bad"); dSys = true; }
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }

    const auto pawn = EntityFromHandle(system, o.entityList, pawnHandle);
    if (!pawn || IsBadReadPtr(pawn, 8)) {
        if (!dPawn) { log.warn("SkinChanger: pawn null/bad"); dPawn = true; }
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }

    if (IsBadReadPtr(reinterpret_cast<const std::uint8_t*>(pawn) + so.weapon_services_offset, sizeof(void*)) ||
        IsBadReadPtr(reinterpret_cast<const std::uint8_t*>(pawn) + so.team_num_offset, sizeof(std::uint8_t))) {
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }

    const auto team = *reinterpret_cast<const std::uint8_t*>(
        reinterpret_cast<const std::uint8_t*>(pawn) + so.team_num_offset);

    const auto wps = *reinterpret_cast<void* const*>(
        reinterpret_cast<const std::uint8_t*>(pawn) + so.weapon_services_offset);
    if (!wps || IsBadReadPtr(wps, sizeof(void*))) {
        if (!dWps) { log.warn("SkinChanger: weapon services null/bad"); dWps = true; }
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }

    const auto weaponsVec = reinterpret_cast<const std::uint8_t*>(wps) + so.my_weapons_offset;
    if (!weaponsVec || IsBadReadPtr(weaponsVec, sizeof(void*) + sizeof(int))) {
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }
    const auto weaponCount = *reinterpret_cast<const int*>(weaponsVec);
    if (weaponCount <= 0 || weaponCount > kMaxWeapons) {
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }
    const auto handles = *reinterpret_cast<const std::uint32_t* const*>(
        weaponsVec + sizeof(void*));
    if (!handles || IsBadReadPtr(handles, static_cast<std::size_t>(weaponCount) * sizeof(std::uint32_t))) {
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }

    void* weaponEntities[kMaxWeapons] = {nullptr};
    int validCount = 0;
    for (int wi = 0; wi < weaponCount; ++wi) {
        const auto entityPtr = EntityFromHandle(system, o.entityList, handles[wi]);
        if (!entityPtr || IsBadReadPtr(entityPtr, 8))
            continue;
        weaponEntities[validCount++] = entityPtr;
    }
    if (validCount == 0) {
        pendingKnifeHandle_ = 0;
        pendingSetModelPath_ = nullptr;
        return;
    }

    const auto& cfg = config::Global();
    const auto& skinTable = skinconfig::SkinTable::Global();

    for (int wi = 0; wi < validCount; ++wi) {
        auto* const weapon = reinterpret_cast<std::uint8_t*>(weaponEntities[wi]);
        if (IsBadReadPtr(weapon, so.attribute_manager_offset + so.item_in_container_offset + sizeof(void*)))
            continue;

        auto* const econView = weapon + so.attribute_manager_offset + so.item_in_container_offset;
        if (IsBadReadPtr(econView, so.item_def_index_offset + sizeof(std::uint32_t)))
            continue;

        const auto defIndex = *reinterpret_cast<const std::uint32_t*>(
            econView + so.item_def_index_offset) & 0xFFFFu;
        if (defIndex == 0)
            continue;

        const char* const wn = weapon_name_from_index(static_cast<int>(defIndex));
        if (!wn)
            continue;

        // Default knife (player's own): config'ten seçilen bıçağa model swap.
        if (config::ConfigStore::Instance().isKnifeEnabled() &&
            (defIndex == kDefaultKnifeDefIndex42 || defIndex == kDefaultKnifeDefIndex59)) {
            const std::uint32_t targetDef = (team == 3) ? cfg.ctKnifeDef : cfg.tKnifeDef;
            const auto* knife = itemcatalog::FindKnifeByDef(targetDef);
            if (knife && defIndex != knife->defIndex && o.updateSubClassValid &&
                !IsBadReadPtr(econView + so.item_quality_offset, sizeof(std::int32_t)) &&
                !IsBadReadPtr(econView + so.account_id_offset, sizeof(std::int32_t)) &&
                !IsBadReadPtr(weapon + so.subclass_id_offset, sizeof(std::uint32_t))) {
                *reinterpret_cast<std::uint16_t*>(econView + so.item_def_index_offset) =
                    knife->defIndex;
                *reinterpret_cast<std::int32_t*>(econView + so.item_quality_offset) = kQuality;
                *reinterpret_cast<std::int32_t*>(econView + so.account_id_offset) =
                    static_cast<std::int32_t>(kAccountId);
                *reinterpret_cast<std::uint32_t*>(weapon + so.subclass_id_offset) =
                    knife->subclassId;

                using UpdateSubClassFn = void (*)(void*);
                reinterpret_cast<UpdateSubClassFn>(o.updateSubClass)(weapon);

                pendingKnifeHandle_ = handles[wi];
                pendingSetModelPath_ = knife->modelPath;
            }
        }

        if (!config::ConfigStore::Instance().isSkinEnabled())
            continue;

        if (IsBadReadPtr(econView + so.item_id_high_offset, sizeof(std::int64_t)))
            continue;

        const auto itemIdHigh = *reinterpret_cast<const std::int64_t*>(
            econView + so.item_id_high_offset);
        if (itemIdHigh == -1LL)
            continue;  // fallback fields already applied this life

        skinconfig::SkinEntry entry;
        if (!skinTable.Get(wn, team, entry))
            continue;  // bu silah config'te yok -> dokunma

        // Capture the vtable of a game-owned attribute so injected entries reuse it.
        AttributeManager::ObserveVTable(econView, so.attribute_list_offset, so.attributes_offset);

        // Fallback fields make the econ layer render the configured skin.
        *reinterpret_cast<std::int32_t*>(weapon + so.fallback_paint_kit_offset) = entry.paint_kit;
        *reinterpret_cast<std::int32_t*>(weapon + so.fallback_seed_offset) = entry.seed;
        *reinterpret_cast<float*>(weapon + so.fallback_wear_offset) = entry.wear;
        if (so.fallback_stat_trak_offset)
            *reinterpret_cast<std::int32_t*>(weapon + so.fallback_stat_trak_offset) = -1;
        *reinterpret_cast<std::int64_t*>(econView + so.item_id_high_offset) = -1LL;

        // Inject real CEconItemAttribute entries (paint/seed/wear) into the item
        // view's attribute list so the paint kit resolves for knives too.
        AttributeManager::Apply(econView, so.attribute_list_offset, so.attributes_offset,
                                entry.paint_kit, entry.seed, entry.wear);

        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "SkinChanger: applied paint=%d seed=%d wear=%.6f to weapon=%s def=%u",
                      static_cast<int>(entry.paint_kit), static_cast<int>(entry.seed),
                      static_cast<double>(entry.wear), wn, static_cast<unsigned>(defIndex));
        log.info(buf);
    }
}

void SkinChanger::RunSetModels() {
    if (!pendingKnifeHandle_ || !pendingSetModelPath_)
        return;

    const auto handle = pendingKnifeHandle_;
    const char* const model = pendingSetModelPath_;
    pendingKnifeHandle_ = 0;
    pendingSetModelPath_ = nullptr;

    const auto& o = ResolveClientOffsets();
    if (!o.setModel || !o.entitySystem)
        return;

    const auto system = ReadPointer(o.entitySystem);
    if (!system || IsBadReadPtr(reinterpret_cast<const void*>(system), sizeof(void*)))
        return;

    void* const weapon = EntityFromHandle(system, o.entityList, handle);
    if (!weapon || IsBadReadPtr(weapon, 8))
        return;

    using SetModelFn = void (*)(void*, const char*);
    reinterpret_cast<SetModelFn>(o.setModel)(weapon, model);

    static Logger log;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "SkinChanger: setModel(%s) on knife handle 0x%X (stage 7)", model, handle);
    log.info(buf);
}