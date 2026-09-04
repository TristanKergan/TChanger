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

struct KnifeModel {
    std::uint16_t defIndex;
    const char* modelPath;
    std::uint32_t subclassId;
};

// Legacy knife table: econ def index, render model path, entity subclass id.
const KnifeModel Knives[] = {
    {500, "weapons/models/knife/knife_bayonet/weapon_knife_bayonet.vmdl", 3933374535u},
    {503, "weapons/models/knife/knife_css/weapon_knife_css.vmdl", 3787235507u},
    {505, "weapons/models/knife/knife_flip/weapon_knife_flip.vmdl", 4046390180u},
    {506, "weapons/models/knife/knife_gut/weapon_knife_gut.vmdl", 2047704618u},
    {507, "weapons/models/knife/knife_karambit/weapon_knife_karambit.vmdl", 1731408398u},
    {508, "weapons/models/knife/knife_m9/weapon_knife_m9.vmdl", 1638561588u},
    {509, "weapons/models/knife/knife_tactical/weapon_knife_tactical.vmdl", 2282479884u},
    {512, "weapons/models/knife/knife_falchion/weapon_knife_falchion.vmdl", 3412259219u},
    {514, "weapons/models/knife/knife_bowie/weapon_knife_bowie.vmdl", 2511498851u},
    {515, "weapons/models/knife/knife_butterfly/weapon_knife_butterfly.vmdl", 1353709123u},
    {516, "weapons/models/knife/knife_push/weapon_knife_push.vmdl", 4269888884u},
    {517, "weapons/models/knife/knife_cord/weapon_knife_cord.vmdl", 1105782941u},
    {518, "weapons/models/knife/knife_canis/weapon_knife_canis.vmdl", 275962944u},
    {519, "weapons/models/knife/knife_ursus/weapon_knife_ursus.vmdl", 1338637359u},
    {521, "weapons/models/knife/knife_outdoor/weapon_knife_outdoor.vmdl", 3206681373u},
    {520, "weapons/models/knife/knife_navaja/weapon_knife_navaja.vmdl", 3230445913u},
    {522, "weapons/models/knife/knife_stiletto/weapon_knife_stiletto.vmdl", 2595277776u},
    {523, "weapons/models/knife/knife_talon/weapon_knife_talon.vmdl", 4029975521u},
    {525, "weapons/models/knife/knife_skeleton/weapon_knife_skeleton.vmdl", 365028728u},
    {526, "weapons/models/knife/knife_kukri/weapon_knife_kukri.vmdl", 3845286452u},
};
constexpr int KnifeCount = sizeof(Knives) / sizeof(Knives[0]);

// Weapon item definition index -> name (config.json "skins" anahtarlarıyla uyumlu).
const char* weapon_name_from_index(int index) {
    switch (index) {
        case 1:   return "deagle";
        case 2:   return "dual_berettas";
        case 3:   return "five_seven";
        case 4:   return "glock";
        case 7:   return "ak47";
        case 8:   return "aug";
        case 9:   return "awp";
        case 10:  return "famas";
        case 11:  return "g3sg1";
        case 13:  return "galilar";
        case 14:  return "m249";
        case 16:  return "m4a4";
        case 17:  return "mac10";
        case 19:  return "p90";
        case 23:  return "mp5sd";
        case 24:  return "ump45";
        case 25:  return "xm1014";
        case 26:  return "bizon";
        case 27:  return "mag7";
        case 28:  return "negev";
        case 29:  return "sawedoff";
        case 30:  return "tec9";
        case 31:  return "zeus";
        case 32:  return "p2000";
        case 33:  return "mp7";
        case 34:  return "mp9";
        case 35:  return "nova";
        case 36:  return "p250";
        case 38:  return "scar20";
        case 39:  return "sg556";
        case 40:  return "ssg08";
        case 42:  return "knife"; // CT knife
        case 59:  return "knife"; // T  Knife
        case 60:  return "m4a1";
        case 61:  return "usp";
        case 63:  return "cz75a";
        case 64:  return "revolver";
        case 500: return "knife"; // Bayonet
        case 503: return "knife"; // Classic
        case 505: return "knife"; // Flip
        case 506: return "knife"; // Gut
        case 507: return "knife"; // Karambit
        case 508: return "knife"; // M9 Bayonet
        case 509: return "knife"; // Huntsman
        case 512: return "knife"; // Falchion
        case 514: return "knife"; // Bowie
        case 515: return "knife"; // Butterfly
        case 516: return "knife"; // Shadow Daggers
        case 517: return "knife"; // Paracord
        case 518: return "knife"; // Survival
        case 519: return "knife"; // Ursus
        case 520: return "knife"; // Navaja
        case 521: return "knife"; // Nomad
        case 522: return "knife"; // Stiletto
        case 523: return "knife"; // Talon
        case 525: return "knife"; // Skeleton
        default:  return nullptr;
    }
}

}  // namespace

export class SkinChanger {
public:
    static SkinChanger& Global();
    void Run();          // stage 6
    void RunSetModels(); // stage 7

private:
    void* pendingSetModelPtr_ = nullptr;
    const char* pendingSetModelPath_ = nullptr;
};

SkinChanger& SkinChanger::Global() {
    static SkinChanger instance;
    return instance;
}

void SkinChanger::Run() {
    static Logger log;
        static bool dResolve = false, dSchema = false, dCtrl = false, dPawnH = false,
                     dSys = false, dPawn = false, dWps = false, dWeapons = false,
                     dKnifeSwap = false, dTeam = false;

    const auto& o = ResolveClientOffsets();
    if (!o.resolved || !o.localPlayerController || !o.entitySystem || !o.setModel) {
        if (!dResolve) { log.error("SkinChanger: client offsets not resolved"); dResolve = true; }
        pendingSetModelPtr_ = nullptr;
        return;
    }

    const auto& so = ResolveSkin();
    if (!so.Ready()) {
        if (!dSchema) { log.error("SkinChanger: schema offsets incomplete"); dSchema = true; }
        pendingSetModelPtr_ = nullptr;
        return;
    }

    if (so.team_num_offset == 0) {
        if (!dTeam) { log.error("SkinChanger: m_iTeamNum offset unresolved"); dTeam = true; }
        pendingSetModelPtr_ = nullptr;
        return;
    }

    // Resolver'ın localPlayerController global'ı +1 RIP quirk'ine tabidir.
    void* controller = TryController(o.localPlayerController, ClientModule);
    if (!controller)
        controller = TryController(o.localPlayerController + 1, ClientModule);
    if (!controller || IsBadReadPtr(reinterpret_cast<const void*>(controller), 8)) {
        if (!dCtrl) { log.warn("SkinChanger: controller null/bad"); dCtrl = true; }
        pendingSetModelPtr_ = nullptr;
        return;
    }

    if (IsBadReadPtr(reinterpret_cast<const std::uint8_t*>(controller) + o.m_hPawn, sizeof(std::uint32_t))) {
        pendingSetModelPtr_ = nullptr;
        return;
    }
    const auto pawnHandle = *reinterpret_cast<const std::uint32_t*>(
        reinterpret_cast<const std::uint8_t*>(controller) + o.m_hPawn);

    const auto system = ReadPointer(o.entitySystem);
    if (!system || IsBadReadPtr(reinterpret_cast<const void*>(system), sizeof(void*))) {
        if (!dSys) { log.warn("SkinChanger: entitySystem null/bad"); dSys = true; }
        pendingSetModelPtr_ = nullptr;
        return;
    }

    const auto pawn = EntityFromHandle(system, o.entityList, pawnHandle);
    if (!pawn || IsBadReadPtr(pawn, 8)) {
        if (!dPawn) { log.warn("SkinChanger: pawn null/bad"); dPawn = true; }
        pendingSetModelPtr_ = nullptr;
        return;
    }

    if (IsBadReadPtr(reinterpret_cast<const std::uint8_t*>(pawn) + so.weapon_services_offset, sizeof(void*)) ||
        IsBadReadPtr(reinterpret_cast<const std::uint8_t*>(pawn) + so.team_num_offset, sizeof(std::uint8_t))) {
        pendingSetModelPtr_ = nullptr;
        return;
    }

    const auto team = *reinterpret_cast<const std::uint8_t*>(
        reinterpret_cast<const std::uint8_t*>(pawn) + so.team_num_offset);

    const auto wps = *reinterpret_cast<void* const*>(
        reinterpret_cast<const std::uint8_t*>(pawn) + so.weapon_services_offset);
    if (!wps || IsBadReadPtr(wps, sizeof(void*))) {
        if (!dWps) { log.warn("SkinChanger: weapon services null/bad"); dWps = true; }
        pendingSetModelPtr_ = nullptr;
        return;
    }

    const auto weaponsVec = reinterpret_cast<const std::uint8_t*>(wps) + so.my_weapons_offset;
    if (!weaponsVec || IsBadReadPtr(weaponsVec, sizeof(void*) + sizeof(int))) {
        pendingSetModelPtr_ = nullptr;
        return;
    }
    const auto weaponCount = *reinterpret_cast<const int*>(weaponsVec);
    if (weaponCount <= 0 || weaponCount > kMaxWeapons) {
        pendingSetModelPtr_ = nullptr;
        return;
    }
    const auto handles = *reinterpret_cast<const std::uint32_t* const*>(
        weaponsVec + sizeof(void*));
    if (!handles || IsBadReadPtr(handles, static_cast<std::size_t>(weaponCount) * sizeof(std::uint32_t))) {
        pendingSetModelPtr_ = nullptr;
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
        pendingSetModelPtr_ = nullptr;
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
            for (int k = 0; k < KnifeCount; ++k) {
                if (Knives[k].defIndex != targetDef)
                    continue;
                const KnifeModel& knife = Knives[k];
                if (defIndex != knife.defIndex && o.updateSubClassValid &&
                    !IsBadReadPtr(econView + so.item_quality_offset, sizeof(std::int32_t)) &&
                    !IsBadReadPtr(econView + so.account_id_offset, sizeof(std::int32_t)) &&
                    !IsBadReadPtr(weapon + so.subclass_id_offset, sizeof(std::uint32_t))) {
                    *reinterpret_cast<std::uint16_t*>(econView + so.item_def_index_offset) =
                        knife.defIndex;
                    *reinterpret_cast<std::int32_t*>(econView + so.item_quality_offset) = kQuality;
                    *reinterpret_cast<std::int32_t*>(econView + so.account_id_offset) =
                        static_cast<std::int32_t>(kAccountId);
                    *reinterpret_cast<std::uint32_t*>(weapon + so.subclass_id_offset) =
                        knife.subclassId;

                    using UpdateSubClassFn = void (*)(void*);
                    reinterpret_cast<UpdateSubClassFn>(o.updateSubClass)(weapon);

                    pendingSetModelPtr_ = weapon;
                    pendingSetModelPath_ = knife.modelPath;
                }
                break;
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
    if (!pendingSetModelPtr_)
        return;

    void* const weapon = pendingSetModelPtr_;
    const char* const model = pendingSetModelPath_;
    pendingSetModelPtr_ = nullptr;
    if (!weapon || !model || IsBadReadPtr(weapon, 8))
        return;

    const auto& o = ResolveClientOffsets();
    if (!o.setModel)
        return;

    using SetModelFn = void (*)(void*, const char*);
    reinterpret_cast<SetModelFn>(o.setModel)(weapon, model);

    static Logger log;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "SkinChanger: setModel(%s) on knife %p (stage 7)", model, weapon);
    log.info(buf);
}
