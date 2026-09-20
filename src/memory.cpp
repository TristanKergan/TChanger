module;
// POSIX + uio başlıkları GMF'te kalır: export module öncesinde include edilmelidir
#include <cerrno>
#include <sys/uio.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <string_view>

export module Memory;

import Modules;

// CGameEntitySystem chunked list walk sabitleri
inline constexpr std::uint32_t IdentitiesPerChunk = 512;
inline constexpr int NumberOfChunks = 64;
inline constexpr std::size_t EntityIdentitySize = 112;
inline constexpr std::size_t EntityIdentityEntityOffset = 0;
inline constexpr std::size_t EntityIdentityHandleOffset = 16;
inline constexpr std::uint32_t HandleValidMask = 0x7FFFu;

// Bogus pointer'ı crash'e yol açmadan güvenle kopyalar (process_vm_readv ile korunur).
export bool SafeRead(std::uintptr_t addr, void* dst, std::size_t size) {
    if (!addr || !dst || size == 0 || (UINTPTR_MAX - addr < size))
        return false;
    struct iovec local = { dst, size };
    struct iovec remote = { reinterpret_cast<void*>(addr), size };
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == static_cast<ssize_t>(size);
}

export std::uintptr_t ReadPointer(std::uintptr_t slot) {
    if (!slot)
        return 0;
    std::uintptr_t val = 0;
    if (!SafeRead(slot, &val, sizeof(val)))
        return 0;
    return val;
}

// Geçersiz adresi process_vm_readv ile güvenle ve deterministik tespit et (crash yerine EFAULT).
export bool IsBadReadPtr(const void* ptr, std::size_t size) {
    if (!ptr || size == 0)
        return true;

    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(ptr);
    if (UINTPTR_MAX - start < size)
        return true;

    // Küçük boyutlar (<= 64 byte) için hızlı yol
    if (size <= 64) {
        char dummy[64];
        struct iovec local = { dummy, size };
        struct iovec remote = { const_cast<void*>(ptr), size };
        return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) != static_cast<ssize_t>(size);
    }

    // Daha büyük bellek aralıkları için sayfa bazlı kontrol [ptr, ptr + size)
    const std::uintptr_t end = start + size;
    std::uintptr_t cur = start;
    char byte = 0;
    struct iovec local = { &byte, 1 };

    while (cur < end) {
        struct iovec remote = { reinterpret_cast<void*>(cur), 1 };
        if (process_vm_readv(getpid(), &local, 1, &remote, 1, 0) != 1)
            return true;
        const std::uintptr_t nextPage = (cur & ~static_cast<std::uintptr_t>(0xFFF)) + 4096;
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

// CEntityHandle -> entity pointer (CGameEntitySystem chunked list walk).
// entityListOffset: CGameEntitySystem::entityList üye offset'i (dinamik çözülür).
export void* EntityFromHandle(std::uintptr_t system, std::int8_t entityListOffset,
                              std::uint32_t handle) {
    if (!system)
        return nullptr;
    const auto index = handle & HandleValidMask;
    if (index == HandleValidMask || index == 0)
        return nullptr;

    const auto list = reinterpret_cast<const std::uint8_t*>(system) + entityListOffset;
    const auto chunkIndex = index / IdentitiesPerChunk;
    if (chunkIndex >= NumberOfChunks)
        return nullptr;
    const auto chunk = *reinterpret_cast<void* const*>(
        list + static_cast<std::size_t>(chunkIndex) * sizeof(void*));
    if (!chunk)
        return nullptr;

    const auto identity = reinterpret_cast<std::uint8_t*>(chunk) +
                          (index % IdentitiesPerChunk) * EntityIdentitySize;
    if (*reinterpret_cast<const std::uint32_t*>(identity + EntityIdentityHandleOffset) != handle)
        return nullptr;
    return *reinterpret_cast<void* const*>(identity + EntityIdentityEntityOffset);
}

// x86_64 rip-relative: hedef = dispKonumu + 4 + trailingBytes + disp32
// trailingBytes: disp32 sonrasında gelen ek instruction baytları (örn. imm8 için 1).
export std::uintptr_t ResolveRipRel(std::uintptr_t dispLoc, std::size_t trailingBytes = 0) {
    if (!dispLoc)
        return 0;
    std::int32_t disp = 0;
    if (!SafeRead(dispLoc, &disp, sizeof(disp)))
        return 0;
    return dispLoc + 4 + trailingBytes + static_cast<std::uintptr_t>(disp);
}

export bool ReadI16(std::uintptr_t addr, std::int16_t* out) { return SafeRead(addr, out, sizeof(*out)); }
export bool ReadI32(std::uintptr_t addr, std::int32_t* out) { return SafeRead(addr, out, sizeof(*out)); }
export bool ReadPtr(std::uintptr_t addr, std::uintptr_t* out) { return SafeRead(addr, out, sizeof(*out)); }

export bool ReadString(std::uintptr_t addr, char* dst, std::size_t cap) {
    if (!addr || !dst || cap == 0)
        return false;
    dst[0] = '\0';
    char chunk[64];
    std::size_t filled = 0;
    for (;;) {
        if (!SafeRead(addr + filled, chunk, sizeof(chunk))) {
            dst[filled < cap ? filled : cap - 1] = '\0';
            return false;
        }
        for (std::size_t i = 0; i < sizeof(chunk); ++i) {
            if (filled + i + 1 >= cap) {
                dst[cap - 1] = '\0';
                return false;
            }
            dst[filled + i] = chunk[i];
            if (chunk[i] == '\0')
                return true;
        }
        filled += sizeof(chunk);
    }
}

// İşaretçiyi okur, vtable'ın belirtilen modülde olduğunu doğrular; geçersizse nullptr döner.
export void* TryController(std::uintptr_t slot, std::string_view moduleName) {
    const auto p = ReadPointer(slot);
    if (!p || IsBadReadPtr(reinterpret_cast<const void*>(p), 8))
        return nullptr;
    const auto vtbl = ReadPointer(p);
    if (!vtbl || !IsAddressInModule(moduleName, vtbl))
        return nullptr;
    return reinterpret_cast<void*>(p);
}
