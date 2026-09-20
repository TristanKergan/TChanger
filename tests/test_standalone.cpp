#include <iostream>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cmath>
#include <bit>
#include <string>
#include <string_view>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>

#include "../src/ipc_protocol.hpp"

import Logger;
import Json;
import ItemCatalog;
import Config;
import SkinConfig;
import RuntimeConfig;
import IpcServer;

// Standalone verification for Hamzex:
// 1. Low-level memory and instruction resolution safety
// 2. ItemCatalog data source of truth (weapons, knives, agents, paint kits)
// 3. Weapon alias and canonicalization
// 4. Wear preset resolution and bounds clamping
// 5. Knife defIndex vs PaintKit collision prevention
// 6. Modern V2 config format parsing and team-aware lookups
// 7. Legacy V1 config format parsing and 100% backward compatibility
// 8. Error handling and disabled weapon toggles

// ---------------------------------------------------------------------------
// Low-level memory & instruction helpers from core
// ---------------------------------------------------------------------------

uintptr_t ResolveRipRelTest(uintptr_t dispLoc, size_t trailingBytes) {
    if (!dispLoc) return 0;
    int32_t disp = 0;
    std::memcpy(&disp, reinterpret_cast<const void*>(dispLoc), sizeof(disp));
    return dispLoc + 4 + trailingBytes + static_cast<uintptr_t>(disp);
}

bool IsBadReadPtrTest(const void* ptr, size_t size) {
    if (!ptr || size == 0) return true;
    const uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
    if (UINTPTR_MAX - start < size)
        return true;
    if (size <= 64) {
        char dummy[64];
        struct iovec local = { dummy, size };
        struct iovec remote = { const_cast<void*>(ptr), size };
        return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) != static_cast<ssize_t>(size);
    }
    const uintptr_t end = start + size;
    uintptr_t cur = start;
    char byte = 0;
    struct iovec local = { &byte, 1 };

    while (cur < end) {
        struct iovec remote = { reinterpret_cast<void*>(cur), 1 };
        if (process_vm_readv(getpid(), &local, 1, &remote, 1, 0) != 1)
            return true;
        const uintptr_t nextPage = (cur & ~static_cast<uintptr_t>(0xFFF)) + 4096;
        if (nextPage >= end) {
            if (cur != end - 1) {
                struct iovec lastRemote = { reinterpret_cast<void*>(end - 1), 1 };
                if (process_vm_readv(getpid(), &local, 1, &lastRemote, 1, 0) != 1)
                    return true;
            }
            break;
        }
        cur = nextPage;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Main test suite
// ---------------------------------------------------------------------------

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "  HAMZEX COMPREHENSIVE OFFLINE TEST SUITE               " << std::endl;
    std::cout << "========================================================" << std::endl;

    // -----------------------------------------------------------------------
    // PART 1: Low-level memory & instruction safety
    // -----------------------------------------------------------------------
    std::cout << "\n[1/8] Running low-level safety & instruction tests..." << std::endl;
    {
        uint8_t insn[7] = { 0x48, 0x89, 0x1D, 0x20, 0x00, 0x00, 0x00 };
        uintptr_t dispLoc = reinterpret_cast<uintptr_t>(&insn[3]);
        uintptr_t target = ResolveRipRelTest(dispLoc, 0);
        uintptr_t expected = dispLoc + 4 + 0x20;
        assert(target == expected);
        std::cout << "  [PASS] ResolveRipRel (trailingBytes = 0)" << std::endl;
    }
    {
        uint8_t insn[8] = { 0x48, 0x83, 0x3D, 0x10, 0x00, 0x00, 0x00, 0x00 };
        uintptr_t dispLoc = reinterpret_cast<uintptr_t>(&insn[3]);
        uintptr_t target = ResolveRipRelTest(dispLoc, 1);
        uintptr_t expected = dispLoc + 5 + 0x10;
        assert(target == expected);
        std::cout << "  [PASS] ResolveRipRel (trailingBytes = 1, cmp imm8)" << std::endl;
    }
    {
        int32_t negDisp = -0x30;
        uint8_t insn[7] = { 0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00 };
        std::memcpy(&insn[3], &negDisp, sizeof(negDisp));
        uintptr_t dispLoc = reinterpret_cast<uintptr_t>(&insn[3]);
        uintptr_t target = ResolveRipRelTest(dispLoc, 0);
        uintptr_t expected = dispLoc + 4 + static_cast<uintptr_t>(negDisp);
        assert(target == expected);
        std::cout << "  [PASS] ResolveRipRel (negative displacement)" << std::endl;
    }
    {
        int validInt = 1337;
        assert(IsBadReadPtrTest(&validInt, sizeof(validInt)) == false);
        assert(IsBadReadPtrTest(nullptr, 4) == true);
        assert(IsBadReadPtrTest(reinterpret_cast<void*>(0x1234), 8) == true);
        assert(IsBadReadPtrTest(reinterpret_cast<void*>(0xFFFFFFFFFFFF0000ULL), 8) == true);
        assert(IsBadReadPtrTest(reinterpret_cast<void*>(0xFFFFFFFFFFFFFFF0ULL), 100) == true);
        assert(IsBadReadPtrTest(reinterpret_cast<void*>(UINTPTR_MAX), 10) == true);
        std::cout << "  [PASS] IsBadReadPtr (valid, null, invalid, overflow)" << std::endl;
    }
    {
        int32_t paintKit = 1171;
        float floatVal = std::bit_cast<float>(paintKit);
        int32_t recovered = std::bit_cast<int32_t>(floatVal);
        assert(recovered == paintKit);
        std::cout << "  [PASS] std::bit_cast round-trip" << std::endl;
    }

    // -----------------------------------------------------------------------
    // PART 2: Weapon alias & canonicalization
    // -----------------------------------------------------------------------
    std::cout << "\n[2/8] Running weapon alias & canonicalization tests..." << std::endl;
    {
        assert(std::string(itemcatalog::CanonicalWeaponName("ak-47")) == "ak47");
        assert(std::string(itemcatalog::CanonicalWeaponName("AK_47")) == "ak47");
        assert(std::string(itemcatalog::CanonicalWeaponName("ak")) == "ak47");

        assert(std::string(itemcatalog::CanonicalWeaponName("m4a1-s")) == "m4a1");
        assert(std::string(itemcatalog::CanonicalWeaponName("m4a1_s")) == "m4a1");
        assert(std::string(itemcatalog::CanonicalWeaponName("M4A1_SILENCER")) == "m4a1");

        assert(std::string(itemcatalog::CanonicalWeaponName("usp-s")) == "usp");
        assert(std::string(itemcatalog::CanonicalWeaponName("USP_S")) == "usp");

        assert(std::string(itemcatalog::CanonicalWeaponName("sg553")) == "sg556");
        assert(std::string(itemcatalog::CanonicalWeaponName("SG-553")) == "sg556");

        assert(std::string(itemcatalog::CanonicalWeaponName("mp5")) == "mp5sd");
        assert(std::string(itemcatalog::CanonicalWeaponName("mp5-sd")) == "mp5sd");

        assert(std::string(itemcatalog::CanonicalWeaponName("scout")) == "ssg08");
        assert(std::string(itemcatalog::CanonicalWeaponName("desert-eagle")) == "deagle");

        // Knife models resolve to "knife" in weapon context
        assert(std::string(itemcatalog::CanonicalWeaponName("butterfly")) == "knife");
        assert(std::string(itemcatalog::CanonicalWeaponName("karambit")) == "knife");
        assert(std::string(itemcatalog::CanonicalWeaponName("m9_bayonet")) == "knife");
        assert(std::string(itemcatalog::CanonicalWeaponName("kukri")) == "knife");

        // Invalid name
        assert(itemcatalog::CanonicalWeaponName("quantum_laser_gun") == nullptr);
        std::cout << "  [PASS] CanonicalWeaponName handles hyphens, case, aliases, and knives" << std::endl;
    }

    // -----------------------------------------------------------------------
    // PART 3: ItemCatalog definitions (Weapons, Knives, Agents)
    // -----------------------------------------------------------------------
    std::cout << "\n[3/8] Running ItemCatalog lookup tests..." << std::endl;
    {
        // Weapons
        const auto* ak = itemcatalog::FindWeaponByDef(7);
        assert(ak && std::string(ak->canonicalName) == "ak47");
        const auto* m4 = itemcatalog::FindWeaponByDef(60);
        assert(m4 && std::string(m4->canonicalName) == "m4a1");
        const auto* usp = itemcatalog::FindWeaponByDef(61);
        assert(usp && std::string(usp->canonicalName) == "usp");

        const auto* byNameAk = itemcatalog::FindWeapon("ak-47");
        assert(byNameAk && byNameAk->defIndex == 7);
        const auto* byNameM4 = itemcatalog::FindWeapon("m4a1-s");
        assert(byNameM4 && byNameM4->defIndex == 60);

        // Knives (20 knives registered)
        const auto* bfk = itemcatalog::FindKnifeByDef(515);
        assert(bfk && std::string(bfk->name) == "butterfly");
        const auto* kara = itemcatalog::FindKnifeByDef(507);
        assert(kara && std::string(kara->name) == "karambit");
        const auto* kukri = itemcatalog::FindKnifeByDef(526);
        assert(kukri && std::string(kukri->name) == "kukri");

        assert(itemcatalog::FindKnife("butterfly")->defIndex == 515);
        assert(itemcatalog::FindKnife("karambit")->defIndex == 507);
        assert(itemcatalog::FindKnife("m9_bayonet")->defIndex == 508);
        assert(itemcatalog::FindKnife("huntsman")->defIndex == 509);
        assert(itemcatalog::FindKnife("daggers")->defIndex == 516);
        assert(itemcatalog::FindKnife("kukri")->defIndex == 526);

        assert(itemcatalog::IsKnifeDefIndex(515) == true);
        assert(itemcatalog::IsKnifeDefIndex(507) == true);
        assert(itemcatalog::IsKnifeDefIndex(42) == true);  // default CT knife
        assert(itemcatalog::IsKnifeDefIndex(59) == true);  // default T knife
        assert(itemcatalog::IsKnifeDefIndex(7) == false);  // AK-47 is not a knife

        // Agents
        const auto* sas = itemcatalog::FindAgentByDef(5601);
        assert(sas && sas->team == 3);
        const auto* phoenix = itemcatalog::FindAgentByDef(5206);
        assert(phoenix && phoenix->team == 2);

        assert(itemcatalog::FindAgent("sas", 3)->def == 5601);
        assert(itemcatalog::FindAgent("phoenix", 2)->def == 5206);
        std::cout << "  [PASS] Weapons, knives, and agents verified against items_game truth" << std::endl;
    }

    // -----------------------------------------------------------------------
    // PART 4: Wear preset resolution & bounds clamping
    // -----------------------------------------------------------------------
    std::cout << "\n[4/8] Running wear preset resolution tests..." << std::endl;
    {
        assert(std::fabs(itemcatalog::ResolveWear("fn", 0.0f) - 0.001f) < 1e-4f);
        assert(std::fabs(itemcatalog::ResolveWear("factory_new", 0.0f) - 0.001f) < 1e-4f);
        assert(std::fabs(itemcatalog::ResolveWear("FACTORY-NEW", 0.0f) - 0.001f) < 1e-4f);
        assert(std::fabs(itemcatalog::ResolveWear("mw", 0.0f) - 0.08f) < 1e-4f);
        assert(std::fabs(itemcatalog::ResolveWear("ft", 0.0f) - 0.20f) < 1e-4f);
        assert(std::fabs(itemcatalog::ResolveWear("ww", 0.0f) - 0.40f) < 1e-4f);
        assert(std::fabs(itemcatalog::ResolveWear("bs", 0.0f) - 0.60f) < 1e-4f);

        // Numeric string
        assert(std::fabs(itemcatalog::ResolveWear("0.1337", 0.0f) - 0.1337f) < 1e-4f);

        // Clamping bounds [0.0, 1.0]
        assert(itemcatalog::ResolveWear("-0.5", 0.0f) == 0.0f);
        assert(itemcatalog::ResolveWear("1.5", 0.0f) == 1.0f);
        std::cout << "  [PASS] Wear strings & presets resolve with proper bounds clamping" << std::endl;
    }

    // -----------------------------------------------------------------------
    // PART 5: Paint kit resolution & knife defIndex collision prevention
    // -----------------------------------------------------------------------
    std::cout << "\n[5/8] Running paint kit resolution & collision prevention tests..." << std::endl;
    {
        assert(itemcatalog::ResolvePaintKit("knife", "fade", 0) == 38);
        assert(itemcatalog::ResolvePaintKit("knife", "doppler_ruby", 0) == 415);
        assert(itemcatalog::ResolvePaintKit("knife", "ruby", 0) == 415);
        assert(itemcatalog::ResolvePaintKit("knife", "doppler_sapphire", 0) == 416);
        assert(itemcatalog::ResolvePaintKit("knife", "case_hardened", 0) == 44);
        assert(itemcatalog::ResolvePaintKit("ak47", "asiimov", 0) == 801);
        assert(itemcatalog::ResolvePaintKit("m4a4", "asiimov", 0) == 255);
        assert(itemcatalog::ResolvePaintKit("awp", "asiimov", 0) == 279);
        assert(itemcatalog::ResolvePaintKit("awp", "dragon_lore", 0) == 344);

        // Weapon-aware skin (Printstream has different IDs per weapon!)
        assert(itemcatalog::ResolvePaintKit("deagle", "printstream", 0) == 962);
        assert(itemcatalog::ResolvePaintKit("m4a1", "printstream", 0) == 984);
        assert(itemcatalog::ResolvePaintKit("usp", "printstream", 0) == 1142);

        // Raw numeric paint kit ID
        assert(itemcatalog::ResolvePaintKit("awp", "1206", 0) == 1206);

        // CRITICAL ROOT CAUSE REPRODUCER & MITIGATION:
        // User mistakenly set knife paint_kit = 515 (Butterfly knife defIndex)
        // Must detect that 515 is a knife defIndex, not a paint kit, and fallback to 38 (Fade)!
        std::int32_t resolvedKit = itemcatalog::ResolvePaintKit("knife", "515", 38);
        assert(resolvedKit == 38);
        std::cout << "  [PASS] Knife defIndex 515 collision on knife correctly detected and guarded (-> 38 Fade)" << std::endl;
        std::cout << "  [PASS] Weapon-aware skins and dictionary lookups resolve properly" << std::endl;
    }

    // -----------------------------------------------------------------------
    // PART 6: JSON Parser capabilities (JSON + JSONC comments)
    // -----------------------------------------------------------------------
    std::cout << "\n[6/8] Running JSON parser & JSONC comment tests..." << std::endl;
    {
        const char* jsonc = R"({
            // Line comment here
            "config_version": 2,
            /* Multi-line comment
               describing knives */
            "knives": {
                "model": "butterfly"
            }
        })";
        json::Value root;
        bool ok = json::Parse(jsonc, std::strlen(jsonc), root);
        assert(ok);
        assert(root.type == json::Type::Object);
        assert(json::GetLong(root, "config_version", 0) == 2);
        const auto* k = root.Find("knives");
        assert(k && k->type == json::Type::Object);
        assert(json::GetString(*k, "model") == "butterfly");
        std::cout << "  [PASS] JSON parser cleanly ignores single-line and multi-line comments" << std::endl;
    }

    // -----------------------------------------------------------------------
    // PART 7: Modern V2 Config & SkinTable end-to-end integration test
    // -----------------------------------------------------------------------
    std::cout << "\n[7/8] Running modern V2 config end-to-end integration tests..." << std::endl;
    {
        const char* v2Json = R"({
            "config_version": 2,
            "knives": {
                "ct": {
                    "model": "karambit",
                    "skin": "doppler_ruby",
                    "wear": "factory_new"
                },
                "t": {
                    "model": "butterfly",
                    "skin": "fade",
                    "wear": "factory_new"
                }
            },
            "agents": {
                "ct": "sas",
                "t": "phoenix"
            },
            "weapons": {
                "ak-47": {
                    "skin": "head_shot",
                    "wear": "factory_new",
                    "seed": 1
                },
                "m4a1-s": {
                    "skin": "printstream",
                    "wear": "factory_new"
                },
                "usp-s": {
                    "skin": "printstream",
                    "wear": "factory_new"
                },
                "awp": {
                    "ct": { "skin": "chrome_cannon" },
                    "t": { "skin": "dragon_lore" }
                },
                "disabled_weapon": {
                    "enabled": false,
                    "skin": "howl"
                }
            }
        })";

        json::Value root;
        bool ok = json::Parse(v2Json, std::strlen(v2Json), root);
        assert(ok);

        // 1. Config resolution
        config::ChangerConfig cfg = config::ParseConfig(root);
        assert(cfg.version == 2);
        assert(cfg.ctKnifeDef == 507);  // Karambit
        assert(cfg.tKnifeDef == 515);   // Butterfly
        assert(cfg.ctAgentDef == 5601); // SAS
        assert(cfg.tAgentDef == 5206);  // Phoenix

        // 2. SkinTable resolution
        skinconfig::SkinTable skinTable;
        assert(skinTable.Load(root));

        // AK-47 check
        skinconfig::SkinEntry akEntry;
        assert(skinTable.Get("ak47", 2, akEntry));
        assert(akEntry.paint_kit == 1171); // Head Shot
        assert(std::fabs(akEntry.wear - 0.001f) < 1e-4f);

        // Alias check: lookup with "ak-47" or "ak47"
        skinconfig::SkinEntry akAlias;
        assert(skinTable.Get("ak-47", 3, akAlias));
        assert(akAlias.paint_kit == 1171);

        // M4A1-S check: lookup with "m4a1" or "m4a1-s"
        skinconfig::SkinEntry m4Entry;
        assert(skinTable.Get("m4a1", 3, m4Entry));
        assert(m4Entry.paint_kit == 984); // Printstream
        assert(skinTable.Get("m4a1-s", 3, m4Entry));
        assert(m4Entry.paint_kit == 984);

        // USP-S check: lookup with "usp" or "usp-s"
        skinconfig::SkinEntry uspEntry;
        assert(skinTable.Get("usp", 3, uspEntry));
        assert(uspEntry.paint_kit == 1142); // Printstream (USP-S ID)
        assert(skinTable.Get("usp-s", 3, uspEntry));
        assert(uspEntry.paint_kit == 1142);

        // Team-split AWP check
        skinconfig::SkinEntry awpCt, awpT;
        assert(skinTable.Get("awp", 3, awpCt));
        assert(awpCt.paint_kit == 1206); // Duality for CT
        assert(skinTable.Get("awp", 2, awpT));
        assert(awpT.paint_kit == 344);  // Dragon Lore for T

        // CT Knife skin check
        skinconfig::SkinEntry knifeCt;
        assert(skinTable.Get("knife", 3, knifeCt));
        assert(knifeCt.paint_kit == 415); // Doppler Ruby

        // T Knife skin check
        skinconfig::SkinEntry knifeT;
        assert(skinTable.Get("knife", 2, knifeT));
        assert(knifeT.paint_kit == 38);  // Fade

        // Disabled weapon check
        skinconfig::SkinEntry disEntry;
        assert(!skinTable.Get("disabled_weapon", 2, disEntry));
        assert(!skinTable.Get("disabled_weapon", 3, disEntry));

        std::cout << "  [PASS] Modern V2 config parsed with knife models, agent defs, team splits, and aliases" << std::endl;
    }

    // -----------------------------------------------------------------------
    // PART 8: Legacy V1 Config & Backward Compatibility
    // -----------------------------------------------------------------------
    std::cout << "\n[8/8] Running legacy V1 config backward compatibility tests..." << std::endl;
    {
        const char* legacyJson = R"({
            "settings": {
                "knife_ct_def": 515,
                "knife_t_def": 515,
                "agent_ct_def": 5601,
                "agent_t_def": 5206
            },
            "skins": {
                "both": {
                    "ak47": { "paint_kit": 1171, "seed": 1, "wear": 0.000001 },
                    "m4a1": { "paint_kit": 984, "seed": 1, "wear": 0.000001 }
                },
                "ct": {
                    "knife": { "paint_kit": 515, "seed": 1, "wear": 0.000001 }
                },
                "t": {
                    "knife": { "paint_kit": 515, "seed": 1, "wear": 0.000001 }
                }
            }
        })";

        json::Value root;
        bool ok = json::Parse(legacyJson, std::strlen(legacyJson), root);
        assert(ok);

        // 1. Legacy settings parsing
        config::ChangerConfig cfg = config::ParseConfig(root);
        assert(cfg.version == 1);
        assert(cfg.ctKnifeDef == 515);
        assert(cfg.tKnifeDef == 515);
        assert(cfg.ctAgentDef == 5601);
        assert(cfg.tAgentDef == 5206);

        // 2. Legacy skins parsing
        skinconfig::SkinTable skinTable;
        assert(skinTable.Load(root));

        skinconfig::SkinEntry ak;
        assert(skinTable.Get("ak47", 2, ak));
        assert(ak.paint_kit == 1171);

        // Legacy knife entry had paint_kit: 515 (Butterfly defIndex)
        // Must be automatically mitigated to 38 (Fade)
        skinconfig::SkinEntry knifeCt, knifeT;
        assert(skinTable.Get("knife", 3, knifeCt));
        assert(knifeCt.paint_kit == 38);
        assert(skinTable.Get("knife", 2, knifeT));
        assert(knifeT.paint_kit == 38);

        std::cout << "  [PASS] Legacy V1 config parsed seamlessly without crash; knife defIndex mitigated" << std::endl;
    }

    // -----------------------------------------------------------------------
    // PART 9: ItemCatalog weapon and knife skin resolution
    // -----------------------------------------------------------------------
    std::cout << "\n[9/11] Running ItemCatalog weapon and knife skin lookups..." << std::endl;
    {
        auto knifeSkins = itemcatalog::GetKnifeSkins();
        assert(knifeSkins.size() >= 30);
        bool foundDoppler = false;
        bool foundFade = false;
        bool foundCaseHardened = false;
        for (const auto& s : knifeSkins) {
            if (std::string_view(s.idName) == "doppler") {
                assert(s.finishId == 415);
                foundDoppler = true;
            }
            if (std::string_view(s.idName) == "fade") {
                assert(s.finishId == 38);
                foundFade = true;
            }
            if (std::string_view(s.idName) == "case_hardened") {
                assert(s.finishId == 44);
                foundCaseHardened = true;
            }
        }
        assert(foundDoppler && foundFade && foundCaseHardened);

        // Weapon-specific skin lookups
        auto akSkins = itemcatalog::GetSkinsForWeapon("ak47");
        assert(!akSkins.empty());
        bool foundAkPrintstream = false;
        bool foundAkAsiimov = false;
        for (const auto& s : akSkins) {
            if (std::string_view(s.idName) == "printstream") {
                assert(s.finishId == 1242);
                foundAkPrintstream = true;
            }
            if (std::string_view(s.idName) == "asiimov") {
                assert(s.finishId == 801);
                foundAkAsiimov = true;
            }
        }
        assert(foundAkPrintstream && foundAkAsiimov);

        auto m4a1sSkins = itemcatalog::GetSkinsForWeapon("m4a1_silencer");
        bool foundM4Printstream = false;
        for (const auto& s : m4a1sSkins) {
            if (std::string_view(s.idName) == "printstream") {
                assert(s.finishId == 984);
                foundM4Printstream = true;
            }
        }
        assert(foundM4Printstream);

        auto deagleSkins = itemcatalog::GetSkinsForWeapon("deagle");
        bool foundDeaglePrintstream = false;
        for (const auto& s : deagleSkins) {
            if (std::string_view(s.idName) == "printstream") {
                assert(s.finishId == 962);
                foundDeaglePrintstream = true;
            }
        }
        assert(foundDeaglePrintstream);

        std::cout << "  [PASS] ItemCatalog knife and weapon skin resolution verified" << std::endl;
    }

    // -----------------------------------------------------------------------
    // PART 10: RuntimeConfig Lock-Free Concurrency & Snapshots
    // -----------------------------------------------------------------------
    std::cout << "\n[10/11] Running RuntimeConfig atomic concurrency & snapshot tests..." << std::endl;
    {
        auto& rcfg = runtimeconfig::RuntimeConfig::Instance();
        auto initialSnap = rcfg.GetSnapshot();
        assert(initialSnap != nullptr);

        std::atomic<bool> stopReaders{false};
        std::atomic<std::size_t> totalReads{0};
        constexpr int kNumReaders = 4;
        std::vector<std::thread> readers;

        for (int i = 0; i < kNumReaders; ++i) {
            readers.emplace_back([&]() {
                std::uint64_t lastVer = 0;
                while (!stopReaders.load(std::memory_order_relaxed)) {
                    auto snap = rcfg.GetSnapshot();
                    assert(snap != nullptr);
                    assert(snap->version >= lastVer);
                    lastVer = snap->version;
                    totalReads.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        // Writer thread simulates rapid config changes via JSON
        for (int i = 0; i < 20; ++i) {
            std::string jsonStr = "{\"config_version\":2,\"weapons\":{\"ak47\":{\"paint_kit\":" +
                                  std::to_string(1000 + i) + ",\"wear\":0.01,\"seed\":" +
                                  std::to_string(i + 1) + "}}}";
            std::string err;
            bool ok = rcfg.UpdateFromJson(jsonStr, err);
            assert(ok);
            usleep(500); // 500us
        }

        stopReaders.store(true, std::memory_order_release);
        for (auto& t : readers) {
            t.join();
        }

        auto finalSnap = rcfg.GetSnapshot();
        assert(finalSnap != nullptr);
        skinconfig::SkinEntry finalAk;
        assert(finalSnap->skins.Get("ak47", 2, finalAk));
        assert(finalAk.paint_kit == 1019);
        assert(finalAk.seed == 20);

        std::cout << "  [PASS] Concurrent lock-free snapshot reads (" << totalReads.load()
                  << " reads across " << kNumReaders << " threads without tearing or deadlocks)" << std::endl;
    }

    // -----------------------------------------------------------------------
    // PART 11: Local IPC Server Loopback & Live Hot-Reload Protocol
    // -----------------------------------------------------------------------
    std::cout << "\n[11/11] Running Local IPC server loopback & hot-reload protocol tests..." << std::endl;
    {
        auto& server = hamzex::server::IpcServer::Global();
        assert(!server.IsRunning());
        bool started = server.Start();
        assert(started);
        assert(server.IsRunning());

        const std::string sPath = server.SocketPath();
        assert(!sPath.empty());

        auto sendIpc = [&](const std::string& req) -> std::string {
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            assert(fd >= 0);
            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::strncpy(addr.sun_path, sPath.c_str(), sizeof(addr.sun_path) - 1);
            int cret = connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
            assert(cret == 0);

            std::string wire = req;
            if (wire.empty() || wire.back() != '\n') wire.push_back('\n');
            ssize_t wn = write(fd, wire.data(), wire.size());
            assert(wn == static_cast<ssize_t>(wire.size()));

            std::string resp;
            char buf[2048];
            while (true) {
                struct pollfd pfd{};
                pfd.fd = fd;
                pfd.events = POLLIN;
                int pret = poll(&pfd, 1, 500);
                if (pret <= 0) break;
                ssize_t rn = read(fd, buf, sizeof(buf));
                if (rn <= 0) break;
                resp.append(buf, static_cast<std::size_t>(rn));
                if (resp.find('\n') != std::string::npos) break;
            }
            close(fd);
            return resp;
        };

        // 1. Ping test
        std::string pingResp = sendIpc("{\"cmd\":\"ping\"}");
        assert(pingResp.find("\"status\":\"ok\"") != std::string::npos);
        assert(pingResp.find("\"config_version\"") != std::string::npos);

        // 2. Get config test
        std::string getResp = sendIpc("{\"cmd\":\"get_config\"}");
        assert(getResp.find("\"status\":\"ok\"") != std::string::npos);
        assert(getResp.find("\"config\"") != std::string::npos);

        // 3. Apply config test (Hot reload!)
        const char* newCfg = R"({
            "cmd": "apply_config",
            "config": {
                "config_version": 2,
                "knives": {
                    "ct": { "model": "karambit", "paint_kit": 415, "wear": 0.01, "seed": 1 }
                },
                "weapons": {
                    "ak47": { "paint_kit": 1242, "wear": 0.01, "seed": 999 }
                }
            }
        })";
        std::string applyResp = sendIpc(newCfg);
        assert(applyResp.find("\"status\":\"ok\"") != std::string::npos);

        // Verify that RuntimeConfig was immediately updated live!
        auto liveSnap = runtimeconfig::RuntimeConfig::Instance().GetSnapshot();
        assert(liveSnap != nullptr);
        assert(liveSnap->changer.ctKnifeDef == 507); // Karambit defIndex
        skinconfig::SkinEntry akEntry;
        assert(liveSnap->skins.Get("ak47", 2, akEntry));
        assert(akEntry.paint_kit == 1242);
        assert(akEntry.seed == 999);

        // 4. Invalid config error handling
        std::string errResp = sendIpc("{\"cmd\":\"apply_config\",\"config\":\"not_an_object\"}");
        assert(errResp.find("\"status\":\"error\"") != std::string::npos);

        // 5. Unknown command error handling
        std::string unkResp = sendIpc("{\"cmd\":\"non_existent_command\"}");
        assert(unkResp.find("\"status\":\"error\"") != std::string::npos);

        // 6. Stop server & verify clean teardown
        server.Stop();
        assert(!server.IsRunning());

        // Test connecting to stopped server should fail
        int testFd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, sPath.c_str(), sizeof(addr.sun_path) - 1);
        int cAfterStop = connect(testFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        assert(cAfterStop != 0); // Must fail
        close(testFd);

        std::cout << "  [PASS] IPC server loopback verified (ping, get_config, apply_config hot reload, validation, clean stop)" << std::endl;
    }

    std::cout << "\n========================================================" << std::endl;
    std::cout << "  [+] ALL TESTS PASSED SUCCESSFULLY!                    " << std::endl;
    std::cout << "========================================================" << std::endl;
    return 0;
}

