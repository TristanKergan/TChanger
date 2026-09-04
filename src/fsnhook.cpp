module;
// POSIX başlıkları GMF'te kalır: export module öncesinde include edilmelidir
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstdio>

import Logger;
import Modules;
import Offsets;
import Memory;
import AgentChanger;
import SkinChanger;

export module FsnHook;

export class FsnHook {
public:
    static bool Install();
    static bool Uninstall();
};

void* g_fsnOriginal = nullptr;  // orijinal FSN fonksiyonu (clientBase + FsnFunctionOffset)
void** g_fsnSlot = nullptr;     // vtable slotu (yazdığımız hücre)
bool g_fsnInstalled = false;

// RELRO korumalı .data.rel.ro sayfasını geçici olarak yazılabilir yap, pointer yaz, geri al.
bool SafeWritePointer(void** address, void* value) {
    const std::size_t pageSize = sysconf(_SC_PAGESIZE);
    const std::uintptr_t pageStart = reinterpret_cast<std::uintptr_t>(address) & ~(pageSize - 1);
    if (mprotect(reinterpret_cast<void*>(pageStart), pageSize, PROT_READ | PROT_WRITE) != 0)
        return false;
    *address = value;
    mprotect(reinterpret_cast<void*>(pageStart), pageSize, PROT_READ);
    return true;
}

// Vtable slotundan çağrılan değiştirme fonksiyonu.
void FsnHookImpl(void* thisptr, int stage, void* arg3, void* arg4) {
    using FsnFn = void (*)(void*, int, void*, void*);
    if (!thisptr)
        return;

    static Logger fsnLog;
    static bool firstEntered = false;
    if (!firstEntered) {
        firstEntered = true;
        fsnLog.info("FsnHookImpl: FIRST ENTERED");
    }

    // Stage 6: skin + knife fallback alanlarını yaz (Fade tüm silahlar).
    if (stage == FRAME_NET_UPDATE_POSTDATAUPDATE_START) {
        SkinChanger::Global().Run();
    }
    // Stage 7: ajan modelini uygula + bıçak SetModel'ı erteleyerek uygula.
    if (stage == FRAME_NET_UPDATE_POSTDATAUPDATE_END) {
        AgentChanger::Global().Run();
        SkinChanger::Global().RunSetModels();
    }

    // Orijinale statik adres üzerinden forwarding.
    if (g_fsnOriginal)
        reinterpret_cast<FsnFn>(g_fsnOriginal)(thisptr, stage, arg3, arg4);
}

bool FsnHook::Install() {
    if (g_fsnInstalled)
        return true;

    static Logger log;
    const auto base = FindModuleBase(ClientModule);
    if (!base) {
        log.error("FSN: libclient.so not loaded");
        return false;
    }

    // 1) Client interface instance'ını CreateInterface ile al.
    void* lib = dlopen(ClientModule.data(), RTLD_NOLOAD | RTLD_NOW);
    if (!lib) {
        log.error("FSN: dlopen(libclient) failed");
        return false;
    }
    using CreateInterfaceFn = void* (*)(const char*, int*);
    auto* CreateInterface = reinterpret_cast<CreateInterfaceFn>(dlsym(lib, "CreateInterface"));
    if (!CreateInterface) {
        log.error("FSN: CreateInterface export not found");
        return false;
    }
    void* pClient = CreateInterface(ClientInterfaceName.data(), nullptr);
    if (!pClient) {
        log.error("FSN: Source2Client002 interface null");
        return false;
    }

    // 2) Canlı vtable.
    void** vtable = *reinterpret_cast<void***>(pClient);
    if (IsBadReadPtr(vtable, sizeof(void*))) {
        log.error("FSN: vtable pointer unreadable");
        return false;
    }

    // 3) FSN index: birincil sabit index (imza taraması yerine). Slot'un hedef
    //    fonksiyonu libclient.so içinde mi diye aşağıda doğrulanır.
    const std::size_t fsnIndex = FsnVtableIndex;
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "FSN: using primary index=%zu", fsnIndex);
        log.info(buf);
    }

    {
        char buf[96];
        std::snprintf(buf, sizeof(buf),
                      "FSN: pClient=%llx vtable=%llx fsnIndex=%zu",
                      static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(pClient)),
                      static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(vtable)),
                      fsnIndex);
        log.info(buf);
    }

    void** slot = &vtable[fsnIndex];
    void* targetFn = *slot;
    if (IsBadReadPtr(targetFn, sizeof(void*))) {
        log.error("FSN: target fn bad/unmapped");
        return false;
    }
    const bool inMod = IsAddressInModule(ClientModule, reinterpret_cast<std::uintptr_t>(targetFn));
    {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "FSN: slot=%llx targetFn=%llx inLibclient=%d",
                      static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(slot)),
                      static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(targetFn)),
                      inMod ? 1 : 0);
        log.info(buf);
    }
    if (!inMod) {
        log.error("FSN: targetFn outside libclient.so (wrong index) — offset drift?");
        return false;
    }

    g_fsnSlot = slot;
    g_fsnOriginal = targetFn;
    if (!SafeWritePointer(slot, reinterpret_cast<void*>(&FsnHookImpl))) {
        log.error("FSN: vtable slot write failed");
        return false;
    }
    g_fsnInstalled = true;
    log.info("FSN vtable hook installed");
    return true;
}

bool FsnHook::Uninstall() {
    if (g_fsnInstalled && g_fsnSlot && g_fsnOriginal)
        SafeWritePointer(g_fsnSlot, g_fsnOriginal);
    g_fsnInstalled = false;
    return true;
}
