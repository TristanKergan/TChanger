module;
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

export module Resolver;

import Logger;
import Modules;
import Memory;
import Offsets;

// Dinamik imza çözücü. Hardcoded offset'ler bu build'de yanlış olduğu için
// (bak: 0x47f3fd9 -> çöp data) libclient.so içinde runtime'da AOB taramasıyla
// global adresleri/fonksiyonu çözer. Legacy parrot projesinden (build 10877660)
// birebir alınan imzalar kullanılır.

struct Sig {
    std::string_view bytes;
    std::size_t offsetToAdd;
};

// x86_64 rip-relative çözümü artık Memory::ResolveRipRel ile yapılıyor.

static std::int32_t ReadInt32At(std::uintptr_t loc) {
    std::int32_t v = 0;
    if (loc)
        std::memcpy(&v, reinterpret_cast<const void*>(loc), sizeof(v));
    return v;
}

static std::int8_t ReadInt8At(std::uintptr_t loc) {
    std::int8_t v = 0;
    if (loc)
        std::memcpy(&v, reinterpret_cast<const void*>(loc), sizeof(v));
    return v;
}

export struct ClientOffsets {
    std::uintptr_t localPlayerController = 0; // absolute addr of CCSPlayerController* global
    std::uintptr_t entitySystem = 0;          // absolute addr of CGameEntitySystem* global
    std::uintptr_t setModel = 0;              // absolute fn addr
    std::uintptr_t updateSubClass = 0;        // absolute fn addr (C_BaseEntity::UpdateSubClassID)
    std::int32_t m_hPawn = 0;
    std::int8_t entityList = 0;
    bool resolved = false;
    bool updateSubClassValid = false;
};

export ClientOffsets& ResolveClientOffsets() {
    static ClientOffsets o;
    static bool done = false;
    if (done)
        return o;
    done = true;

    static Logger log;
    log.info("ResolveClientOffsets: scanning libclient.so (dynamic signatures)");

    // kSetModel (Function)
    {
        const Sig s{ "55 48 89 E5 53 48 89 FB 48 83 EC 08 48 8D 05 ? ? ? ? 48 8B 38 48 8B 07 FF 50 68", 0 };
        const auto m = PatternScan(ClientModule, s.bytes);
        o.setModel = m;
        if (!m)
            log.error("ResolveClientOffsets: kSetModel NOT FOUND");
    }
    // kLocalPlayerController (RipRelative) — 1 byte imm8 follows disp32
    {
        const Sig s{ "48 83 3D ? ? ? ? ? 0F 95 C0 C3", 3 };
        const auto m = PatternScan(ClientModule, s.bytes);
        o.localPlayerController = m ? ResolveRipRel(m + s.offsetToAdd, 1) : 0;
        if (!m)
            log.error("ResolveClientOffsets: kLocalPlayerController NOT FOUND");
    }
    // kEntitySystem (RipRelative)
    {
        const Sig s{ "4C 63 ? ? ? ? ? 48 89 1D ? ? ? ?", 10 };
        const auto m = PatternScan(ClientModule, s.bytes);
        o.entitySystem = m ? ResolveRipRel(m + s.offsetToAdd) : 0;
        if (!m)
            log.error("ResolveClientOffsets: kEntitySystem NOT FOUND");
    }
    // kBasePawnHandle (Int32)
    {
        const Sig s{ "84 C0 75 ? 8B 8F ? ? ? ?", 6 };
        const auto m = PatternScan(ClientModule, s.bytes);
        o.m_hPawn = m ? ReadInt32At(m + s.offsetToAdd) : 0;
        if (!m)
            log.error("ResolveClientOffsets: kBasePawnHandle NOT FOUND");
    }
    // kEntityListOffset (Int8)
    {
        const Sig s{ "4C 8D 6F ? 41 54 53 48 89 FB 48 83 EC ? 48 89 07 48", 3 };
        const auto m = PatternScan(ClientModule, s.bytes);
        o.entityList = m ? ReadInt8At(m + s.offsetToAdd) : 0;
        if (!m)
            log.error("ResolveClientOffsets: kEntityListOffset NOT FOUND");
    }
    // kUpdateSubClass (Function) — for knife model swap
    {
        const Sig s{ "55 48 89 E5 41 57 49 89 FF 41 56 41 55 41 54 53 48 81 EC 48 01 00 00 48 8B 07", 0 };
        const auto m = PatternScan(ClientModule, s.bytes);
        o.updateSubClass = m;
        if (!m)
            log.error("ResolveClientOffsets: kUpdateSubClass NOT FOUND");
    }

    const bool ok =
        o.localPlayerController &&
        o.entitySystem &&
        o.setModel &&
        IsAddressInModule(ClientModule, o.localPlayerController) &&
        IsAddressInModule(ClientModule, o.entitySystem) &&
        IsAddressInModule(ClientModule, o.setModel);
    o.resolved = ok;
    o.updateSubClassValid = o.updateSubClass &&
                            IsAddressInModule(ClientModule, o.updateSubClass);

    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "ResolveClientOffsets: resolved=%d lpc=%llx es=%llx setmodel=%llx m_hPawn=%d entityList=%d",
                  ok ? 1 : 0,
                  static_cast<unsigned long long>(o.localPlayerController),
                  static_cast<unsigned long long>(o.entitySystem),
                  static_cast<unsigned long long>(o.setModel),
                  static_cast<int>(o.m_hPawn),
                  static_cast<int>(o.entityList));
    log.info(buf);
    return o;
}
