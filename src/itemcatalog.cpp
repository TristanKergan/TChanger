module;
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

export module ItemCatalog;

import Logger;

namespace {

std::string ToLower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c >= 'A' && c <= 'Z')
            out.push_back(static_cast<char>(c + ('a' - 'A')));
        else if (c == '-' || c == ' ')
            out.push_back('_');
        else
            out.push_back(c);
    }
    return out;
}

bool IsDigitsOnly(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

}  // namespace

export namespace itemcatalog {

struct KnifeModel {
    std::uint16_t defIndex;
    const char* name;
    const char* modelPath;
    std::uint32_t subclassId;
};

struct AgentModel {
    std::uint32_t def;
    const char* name;
    std::uint8_t team;  // 2 = T, 3 = CT, 0 = Any
    const char* modelPath;
};

struct WeaponInfo {
    std::uint32_t defIndex;
    const char* canonicalName;
    const char* displayName;
};

struct PaintKitMapping {
    const char* name;
    std::int32_t id;
};

// 20 knives from CS2 Source 2
inline constexpr KnifeModel Knives[] = {
    {500, "bayonet", "weapons/models/knife/knife_bayonet/weapon_knife_bayonet.vmdl", 3933374535u},
    {503, "classic", "weapons/models/knife/knife_css/weapon_knife_css.vmdl", 3787235507u},
    {505, "flip", "weapons/models/knife/knife_flip/weapon_knife_flip.vmdl", 4046390180u},
    {506, "gut", "weapons/models/knife/knife_gut/weapon_knife_gut.vmdl", 2047704618u},
    {507, "karambit", "weapons/models/knife/knife_karambit/weapon_knife_karambit.vmdl", 1731408398u},
    {508, "m9", "weapons/models/knife/knife_m9/weapon_knife_m9.vmdl", 1638561588u},
    {509, "huntsman", "weapons/models/knife/knife_tactical/weapon_knife_tactical.vmdl", 2282479884u},
    {512, "falchion", "weapons/models/knife/knife_falchion/weapon_knife_falchion.vmdl", 3412259219u},
    {514, "bowie", "weapons/models/knife/knife_bowie/weapon_knife_bowie.vmdl", 2511498851u},
    {515, "butterfly", "weapons/models/knife/knife_butterfly/weapon_knife_butterfly.vmdl", 1353709123u},
    {516, "shadow_daggers", "weapons/models/knife/knife_push/weapon_knife_push.vmdl", 4269888884u},
    {517, "paracord", "weapons/models/knife/knife_cord/weapon_knife_cord.vmdl", 1105782941u},
    {518, "survival", "weapons/models/knife/knife_canis/weapon_knife_canis.vmdl", 275962944u},
    {519, "ursus", "weapons/models/knife/knife_ursus/weapon_knife_ursus.vmdl", 1338637359u},
    {520, "navaja", "weapons/models/knife/knife_navaja/weapon_knife_navaja.vmdl", 3230445913u},
    {521, "nomad", "weapons/models/knife/knife_outdoor/weapon_knife_outdoor.vmdl", 3206681373u},
    {522, "stiletto", "weapons/models/knife/knife_stiletto/weapon_knife_stiletto.vmdl", 2595277776u},
    {523, "talon", "weapons/models/knife/knife_talon/weapon_knife_talon.vmdl", 4029975521u},
    {525, "skeleton", "weapons/models/knife/knife_skeleton/weapon_knife_skeleton.vmdl", 365028728u},
    {526, "kukri", "weapons/models/knife/knife_kukri/weapon_knife_kukri.vmdl", 3845286452u},
};
inline constexpr std::size_t KnifeCount = sizeof(Knives) / sizeof(Knives[0]);

// Agents from CS2
inline constexpr AgentModel Agents[] = {
    {5601, "sas", 3, "agents/models/ctm_sas/ctm_sas_variantf.vmdl"},
    {5602, "sas_officer", 3, "agents/models/ctm_sas/ctm_sas_variantg.vmdl"},
    {5206, "phoenix", 2, "agents/models/tm_phoenix/tm_phoenix_variantf.vmdl"},
    {5205, "phoenix_enforcer", 2, "agents/models/tm_phoenix/tm_phoenix_varianth.vmdl"},
    {5207, "phoenix_slingshot", 2, "agents/models/tm_phoenix/tm_phoenix_variantg.vmdl"},
    {5208, "phoenix_soldier", 2, "agents/models/tm_phoenix/tm_phoenix_varianti.vmdl"},
    {5305, "fbi", 3, "agents/models/ctm_fbi/ctm_fbi_variantf.vmdl"},
    {5306, "fbi_operator", 3, "agents/models/ctm_fbi/ctm_fbi_variantg.vmdl"},
    {5307, "fbi_special", 3, "agents/models/ctm_fbi/ctm_fbi_variantg.vmdl"},
    {5308, "fbi_sniper", 3, "agents/models/ctm_fbi/ctm_fbi_variantb.vmdl"},
    {4711, "swat", 3, "agents/models/ctm_swat/ctm_swat_variante.vmdl"},
    {4712, "swat_lieutenant", 3, "agents/models/ctm_swat/ctm_swat_variantf.vmdl"},
    {4713, "swat_chem", 3, "agents/models/ctm_swat/ctm_swat_variantg.vmdl"},
    {4714, "swat_sergeant", 3, "agents/models/ctm_swat/ctm_swat_varianth.vmdl"},
    {4715, "swat_medic", 3, "agents/models/ctm_swat/ctm_swat_varianti.vmdl"},
    {4716, "swat_comm", 3, "agents/models/ctm_swat/ctm_swat_variantj.vmdl"},
    {4756, "swat_commander", 3, "agents/models/ctm_swat/ctm_swat_variantk.vmdl"},
    {4619, "st6_buckshot", 3, "agents/models/ctm_st6/ctm_st6_variantj.vmdl"},
    {4680, "st6_seal", 3, "agents/models/ctm_st6/ctm_st6_variantl.vmdl"},
    {5400, "st6_soldier", 3, "agents/models/ctm_st6/ctm_st6_variantk.vmdl"},
    {5401, "st6_medic", 3, "agents/models/ctm_st6/ctm_st6_variante.vmdl"},
    {5402, "st6_sniper", 3, "agents/models/ctm_st6/ctm_st6_variantg.vmdl"},
    {5403, "st6_officer", 3, "agents/models/ctm_st6/ctm_st6_variantm.vmdl"},
    {5404, "st6_operator", 3, "agents/models/ctm_st6/ctm_st6_varianti.vmdl"},
    {5405, "st6_recon", 3, "agents/models/ctm_st6/ctm_st6_variantn.vmdl"},
    {4749, "gendarmerie", 3, "agents/models/ctm_gendarmerie/ctm_gendarmerie_varianta.vmdl"},
    {4750, "gendarmerie_medic", 3, "agents/models/ctm_gendarmerie/ctm_gendarmerie_variantb.vmdl"},
    {4751, "gendarmerie_officer", 3, "agents/models/ctm_gendarmerie/ctm_gendarmerie_variantc.vmdl"},
    {4752, "gendarmerie_commander", 3, "agents/models/ctm_gendarmerie/ctm_gendarmerie_variantd.vmdl"},
    {4753, "gendarmerie_captain", 3, "agents/models/ctm_gendarmerie/ctm_gendarmerie_variante.vmdl"},
    {4757, "diver", 3, "agents/models/ctm_diver/ctm_diver_varianta.vmdl"},
    {4771, "diver_frogman", 3, "agents/models/ctm_diver/ctm_diver_variantb.vmdl"},
    {4772, "diver_elite", 3, "agents/models/ctm_diver/ctm_diver_variantc.vmdl"},
    {5105, "leet", 2, "agents/models/tm_leet/tm_leet_variantg.vmdl"},
    {5106, "leet_rebel", 2, "agents/models/tm_leet/tm_leet_varianth.vmdl"},
    {5107, "leet_soldier", 2, "agents/models/tm_leet/tm_leet_varianti.vmdl"},
    {5108, "leet_commander", 2, "agents/models/tm_leet/tm_leet_variantf.vmdl"},
    {5109, "leet_elite", 2, "agents/models/tm_leet/tm_leet_variantj.vmdl"},
    {5500, "balkan", 2, "agents/models/tm_balkan/tm_balkan_variantf.vmdl"},
    {5501, "balkan_rebel", 2, "agents/models/tm_balkan/tm_balkan_varianti.vmdl"},
    {5502, "balkan_sniper", 2, "agents/models/tm_balkan/tm_balkan_variantg.vmdl"},
    {5503, "balkan_commander", 2, "agents/models/tm_balkan/tm_balkan_variantj.vmdl"},
    {5504, "balkan_soldier", 2, "agents/models/tm_balkan/tm_balkan_varianth.vmdl"},
    {5505, "balkan_scout", 2, "agents/models/tm_balkan/tm_balkan_variantl.vmdl"},
    {4718, "balkan_heavy", 2, "agents/models/tm_balkan/tm_balkan_variantk.vmdl"},
    {4726, "professional", 2, "agents/models/tm_professional/tm_professional_varf.vmdl"},
    {4727, "professional_safecracker", 2, "agents/models/tm_professional/tm_professional_varg.vmdl"},
    {4728, "professional_heist", 2, "agents/models/tm_professional/tm_professional_varh.vmdl"},
    {4730, "professional_operator", 2, "agents/models/tm_professional/tm_professional_varj.vmdl"},
    {4732, "professional_hitman", 2, "agents/models/tm_professional/tm_professional_vari.vmdl"},
    {4733, "professional_f1", 2, "agents/models/tm_professional/tm_professional_varf1.vmdl"},
    {4734, "professional_f2", 2, "agents/models/tm_professional/tm_professional_varf2.vmdl"},
    {4735, "professional_f3", 2, "agents/models/tm_professional/tm_professional_varf3.vmdl"},
    {4736, "professional_f4", 2, "agents/models/tm_professional/tm_professional_varf4.vmdl"},
    {4613, "professional_f5", 2, "agents/models/tm_professional/tm_professional_varf5.vmdl"},
    {4773, "jungle", 2, "agents/models/tm_jungle_raider/tm_jungle_raider_varianta.vmdl"},
    {4774, "jungle_trapper", 2, "agents/models/tm_jungle_raider/tm_jungle_raider_variantb.vmdl"},
    {4775, "jungle_rebel", 2, "agents/models/tm_jungle_raider/tm_jungle_raider_variantc.vmdl"},
    {4776, "jungle_soldier", 2, "agents/models/tm_jungle_raider/tm_jungle_raider_variantd.vmdl"},
    {4777, "jungle_commander", 2, "agents/models/tm_jungle_raider/tm_jungle_raider_variante.vmdl"},
    {4778, "jungle_warlord", 2, "agents/models/tm_jungle_raider/tm_jungle_raider_variantf.vmdl"},
    {4780, "jungle_b2", 2, "agents/models/tm_jungle_raider/tm_jungle_raider_variantb2.vmdl"},
    {4781, "jungle_f2", 2, "agents/models/tm_jungle_raider/tm_jungle_raider_variantf2.vmdl"},
};
inline constexpr std::size_t AgentCount = sizeof(Agents) / sizeof(Agents[0]);

// Weapons table
inline constexpr WeaponInfo Weapons[] = {
    {1, "deagle", "Desert Eagle"},
    {2, "dual_berettas", "Dual Berettas"},
    {3, "five_seven", "Five-SeveN"},
    {4, "glock", "Glock-18"},
    {7, "ak47", "AK-47"},
    {8, "aug", "AUG"},
    {9, "awp", "AWP"},
    {10, "famas", "FAMAS"},
    {11, "g3sg1", "G3SG1"},
    {13, "galilar", "Galil AR"},
    {14, "m249", "M249"},
    {16, "m4a4", "M4A4"},
    {17, "mac10", "MAC-10"},
    {19, "p90", "P90"},
    {23, "mp5sd", "MP5-SD"},
    {24, "ump45", "UMP-45"},
    {25, "xm1014", "XM1014"},
    {26, "bizon", "PP-Bizon"},
    {27, "mag7", "MAG-7"},
    {28, "negev", "Negev"},
    {29, "sawedoff", "Sawed-Off"},
    {30, "tec9", "Tec-9"},
    {31, "zeus", "Zeus x27"},
    {32, "p2000", "P2000"},
    {33, "mp7", "MP7"},
    {34, "mp9", "MP9"},
    {35, "nova", "Nova"},
    {36, "p250", "P250"},
    {38, "scar20", "SCAR-20"},
    {39, "sg556", "SG 553"},
    {40, "ssg08", "SSG 08"},
    {42, "knife", "Default CT Knife"},
    {59, "knife", "Default T Knife"},
    {60, "m4a1", "M4A1-S"},
    {61, "usp", "USP-S"},
    {63, "cz75a", "CZ75-Auto"},
    {64, "revolver", "R8 Revolver"},
};
inline constexpr std::size_t WeaponCount = sizeof(Weapons) / sizeof(Weapons[0]);

// Known popular Paint Kits (Finish Catalog IDs from Valve CS2 items_game.txt)
inline constexpr PaintKitMapping PopularPaintKits[] = {
    // Universal Knives & Weapons
    {"fade", 38},
    {"case_hardened", 44},
    {"ch", 44},
    {"slaughter", 70},
    {"crimson_web", 98},
    {"cw", 98},
    {"blue_steel", 42},
    {"night", 40},
    {"stained", 43},
    {"safari_mesh", 72},
    {"boreal_forest", 77},
    {"urban_masked", 143},
    {"forest_ddpat", 5},
    {"tiger_tooth", 409},
    {"tt", 409},
    {"marble_fade", 413},
    {"mf", 413},
    {"rust_coat", 414},
    {"lore", 573},
    {"autotronic", 578},
    {"black_laminate", 580},
    {"freehand", 582},
    // Doppler series
    {"doppler_ruby", 415},
    {"ruby", 415},
    {"doppler_sapphire", 416},
    {"sapphire", 416},
    {"doppler_black_pearl", 417},
    {"black_pearl", 417},
    {"doppler_phase1", 418},
    {"phase1", 418},
    {"doppler_phase2", 419},
    {"phase2", 419},
    {"doppler_phase3", 420},
    {"phase3", 420},
    {"doppler_phase4", 421},
    {"phase4", 421},
    {"doppler", 419},
    // Gamma Doppler series
    {"gamma_emerald", 568},
    {"emerald", 568},
    {"gamma_phase1", 569},
    {"gamma_phase2", 570},
    {"gamma_phase3", 571},
    {"gamma_phase4", 572},
    {"gamma_doppler", 568},
    // Guns iconic skins
    {"head_shot", 1171},
    {"vogue", 963},
    {"chrome_cannon", 1206},
    {"duality", 1222},
    {"dragonfire", 624},
    {"asiimov", 255},
    {"vulcan", 302},
    {"redline", 282},
    {"slate", 1035},
    {"fire_serpent", 180},
    {"the_empress", 675},
    {"empress", 675},
    {"bloodsport", 597},
    {"hyper_beast", 430},
    {"dragon_lore", 344},
    {"dlore", 344},
    {"medusa", 448},
    {"containment_breach", 845},
    {"neo_noir", 653},
    {"kill_confirmed", 504},
    {"water_elemental", 353},
    {"blaze", 37},
    {"code_red", 757},
    {"howl", 309},
    {"temukau", 1175},
    {"mecha_industries", 556},
    {"golden_coil", 497},
    {"player_two", 946},
    {"black_lotus", 10041},
    {"olympus", 10043},
};
inline constexpr std::size_t PopularPaintKitCount = sizeof(PopularPaintKits) / sizeof(PopularPaintKits[0]);

const KnifeModel* FindKnife(std::string_view nameOrId);

const char* CanonicalWeaponName(std::string_view input) {
    const std::string s = ToLower(input);
    std::string sc;
    sc.reserve(s.size());
    for (char c : s) {
        if (c != '_') sc.push_back(c);
    }

    if (sc == "ak47" || sc == "ak") return "ak47";
    if (sc == "m4a4") return "m4a4";
    if (sc == "m4a1" || sc == "m4a1s" || sc == "m4a1silencer") return "m4a1";
    if (sc == "usp" || sc == "usps" || sc == "uspsilencer") return "usp";
    if (sc == "deagle" || sc == "deserteagle") return "deagle";
    if (sc == "awp") return "awp";
    if (sc == "glock" || sc == "glock18") return "glock";
    if (sc == "ssg08" || sc == "scout") return "ssg08";
    if (sc == "sg556" || sc == "sg553" || sc == "sg") return "sg556";
    if (sc == "mp5sd" || sc == "mp5") return "mp5sd";
    if (sc == "galilar" || sc == "galil") return "galilar";
    if (sc == "famas") return "famas";
    if (sc == "aug") return "aug";
    if (sc == "p250") return "p250";
    if (sc == "fiveseven" || sc == "fn57") return "five_seven";
    if (sc == "tec9") return "tec9";
    if (sc == "cz75a" || sc == "cz75" || sc == "cz") return "cz75a";
    if (sc == "dualberettas" || sc == "duals" || sc == "dualies") return "dual_berettas";
    if (sc == "p2000" || sc == "p2k") return "p2000";
    if (sc == "revolver" || sc == "r8") return "revolver";
    if (sc == "mac10") return "mac10";
    if (sc == "mp9") return "mp9";
    if (sc == "mp7") return "mp7";
    if (sc == "ump45" || sc == "ump") return "ump45";
    if (sc == "p90") return "p90";
    if (sc == "bizon" || sc == "ppbizon") return "bizon";
    if (sc == "nova") return "nova";
    if (sc == "xm1014" || sc == "xm") return "xm1014";
    if (sc == "mag7") return "mag7";
    if (sc == "sawedoff") return "sawedoff";
    if (sc == "m249") return "m249";
    if (sc == "negev") return "negev";
    if (sc == "scar20") return "scar20";
    if (sc == "g3sg1") return "g3sg1";
    if (sc == "zeus" || sc == "taser") return "zeus";
    if (sc == "knife") return "knife";

    // Direct check in Weapons table
    for (const auto& w : Weapons) {
        if (s == w.canonicalName || sc == w.canonicalName) return w.canonicalName;
    }
    // Check in Knives table / aliases (if user wrote knife by specific name or alias, e.g. "butterfly", "m9_bayonet")
    if (FindKnife(input) || FindKnife(s) || FindKnife(sc)) return "knife";
    return nullptr;
}

const WeaponInfo* FindWeaponByDef(std::uint32_t defIndex) {
    for (const auto& w : Weapons) {
        if (w.defIndex == defIndex)
            return &w;
    }
    return nullptr;
}

const WeaponInfo* FindWeapon(std::string_view nameOrId) {
    if (nameOrId.empty()) return nullptr;
    if (IsDigitsOnly(nameOrId)) {
        std::uint32_t def = static_cast<std::uint32_t>(std::strtoul(nameOrId.data(), nullptr, 10));
        return FindWeaponByDef(def);
    }
    const char* canon = CanonicalWeaponName(nameOrId);
    if (!canon) return nullptr;
    for (const auto& w : Weapons) {
        if (std::strcmp(canon, w.canonicalName) == 0)
            return &w;
    }
    return nullptr;
}

const KnifeModel* FindKnifeByDef(std::uint32_t def) {
    for (const auto& k : Knives) {
        if (k.defIndex == def)
            return &k;
    }
    return nullptr;
}

const KnifeModel* FindKnife(std::string_view nameOrId) {
    if (nameOrId.empty()) return nullptr;
    if (IsDigitsOnly(nameOrId)) {
        std::uint32_t def = static_cast<std::uint32_t>(std::strtoul(nameOrId.data(), nullptr, 10));
        return FindKnifeByDef(def);
    }
    const std::string s = ToLower(nameOrId);
    std::string sc;
    sc.reserve(s.size());
    for (char c : s) {
        if (c != '_') sc.push_back(c);
    }

    for (const auto& k : Knives) {
        if (s == k.name || sc == k.name) return &k;
    }
    // Aliases
    if (sc == "css" || sc == "classic") return FindKnifeByDef(503);
    if (sc == "tactical" || sc == "huntsman") return FindKnifeByDef(509);
    if (sc == "push" || sc == "daggers" || sc == "shadowdaggers") return FindKnifeByDef(516);
    if (sc == "cord" || sc == "paracord") return FindKnifeByDef(517);
    if (sc == "canis" || sc == "survival") return FindKnifeByDef(518);
    if (sc == "outdoor" || sc == "nomad") return FindKnifeByDef(521);
    if (sc == "m9" || sc == "m9bayonet") return FindKnifeByDef(508);
    if (sc == "skeleton") return FindKnifeByDef(525);
    if (sc == "kukri") return FindKnifeByDef(526);
    return nullptr;
}

bool IsKnifeDefIndex(std::uint32_t def) {
    return FindKnifeByDef(def) != nullptr || def == 42 || def == 59;
}

const AgentModel* FindAgentByDef(std::uint32_t def) {
    for (const auto& a : Agents) {
        if (a.def == def)
            return &a;
    }
    return nullptr;
}

const AgentModel* FindAgent(std::string_view nameOrId, std::uint8_t team) {
    if (nameOrId.empty()) return nullptr;
    if (IsDigitsOnly(nameOrId)) {
        std::uint32_t def = static_cast<std::uint32_t>(std::strtoul(nameOrId.data(), nullptr, 10));
        return FindAgentByDef(def);
    }
    const std::string s = ToLower(nameOrId);
    for (const auto& a : Agents) {
        if (team != 0 && a.team != 0 && a.team != team)
            continue;
        if (s == a.name)
            return &a;
    }
    // Fallback prefix match (e.g. "sas" matches 5601, "phoenix" matches 5206)
    for (const auto& a : Agents) {
        if (team != 0 && a.team != 0 && a.team != team)
            continue;
        if (std::string_view(a.name).starts_with(s))
            return &a;
    }
    return nullptr;
}

std::int32_t ResolvePaintKit(std::string_view weapon, std::string_view skinOrId, std::int32_t dflt) {
    if (skinOrId.empty())
        return dflt;

    // 1. Direct number check
    if (IsDigitsOnly(skinOrId)) {
        long val = std::strtol(skinOrId.data(), nullptr, 10);
        if (val > 0) {
            // Validation check: did the user pass a knife defIndex as paint kit?
            if (weapon == "knife" && IsKnifeDefIndex(static_cast<std::uint32_t>(val))) {
                static Logger log;
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                              "ItemCatalog: WARNING - knife paint_kit '%ld' is a knife defIndex (e.g. Butterfly), NOT a finish ID! Using Fade (38).",
                              val);
                log.warn(buf);
                return 38; // Default to Fade to prevent broken texture
            }
            return static_cast<std::int32_t>(val);
        }
    }

    const std::string s = ToLower(skinOrId);

    // 2. Weapon-aware skins (different finish catalog IDs per weapon)
    if (s == "printstream") {
        if (weapon == "deagle") return 962;
        if (weapon == "m4a1") return 984;
        if (weapon == "usp") return 1142;
        return 984;
    }
    if (s == "asiimov") {
        if (weapon == "m4a4") return 255;
        if (weapon == "awp") return 279;
        if (weapon == "ak47") return 801;
        if (weapon == "p90") return 359;
        if (weapon == "p250") return 551;
        return 255;
    }
    if (s == "hyper_beast") {
        if (weapon == "m4a1") return 430;
        if (weapon == "awp") return 475;
        if (weapon == "nova") return 537;
        if (weapon == "five_seven") return 574;
        return 430;
    }
    if (s == "neo_noir") {
        if (weapon == "usp") return 653;
        if (weapon == "awp") return 803;
        if (weapon == "m4a4") return 988;
        if (weapon == "glock") return 1132;
        return 653;
    }

    // 3. Known dictionary lookup
    for (const auto& pk : PopularPaintKits) {
        if (s == pk.name)
            return pk.id;
    }

    static Logger log;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "ItemCatalog: unknown skin name '%.64s', using fallback %d",
                  s.c_str(), dflt);
    log.warn(buf);
    return dflt;
}

float ResolveWear(std::string_view wearStr, float dflt) {
    if (wearStr.empty()) return dflt;
    const std::string s = ToLower(wearStr);
    if (s == "fn" || s == "factory_new") return 0.001f;
    if (s == "mw" || s == "minimal_wear") return 0.08f;
    if (s == "ft" || s == "field_tested") return 0.20f;
    if (s == "ww" || s == "well_worn") return 0.40f;
    if (s == "bs" || s == "battle_scarred") return 0.60f;

    char* end = nullptr;
    double d = std::strtod(wearStr.data(), &end);
    if (end != wearStr.data()) {
        float f = static_cast<float>(d);
        if (f < 0.0f) f = 0.0f;
        if (f > 1.0f) f = 1.0f;
        return f;
    }
    return dflt;
}

std::int32_t ResolveSeed(std::string_view seedStr, std::int32_t dflt) {
    if (seedStr.empty()) return dflt;
    char* end = nullptr;
    long v = std::strtol(seedStr.data(), &end, 10);
    if (end != seedStr.data()) {
        if (v < 0) v = 0;
        if (v > 65535) v = 65535;
        return static_cast<std::int32_t>(v);
    }
    return dflt;
}

}  // namespace itemcatalog
