module;
#include <link.h>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

export module Modules;

struct SearchContext {
    std::string_view targetName;
    std::uintptr_t baseAddress = 0;
};

const char* GetFilename(const char* path) {
    const char* filename = std::strrchr(path, '/');
    return filename ? filename + 1 : path;
}

int DlCallback(struct dl_phdr_info* info, size_t size, void* data) {
    auto* ctx = reinterpret_cast<SearchContext*>(data);
    const char* filename = GetFilename(info->dlpi_name);
    if (ctx->targetName == filename) {
        ctx->baseAddress = info->dlpi_addr;
        return 1;
    }
    return 0;
}

// Modül bulucu (dl_iterate_phdr tabanlı)
export std::uintptr_t FindModuleBase(const std::string_view needle) {
    SearchContext ctx{ needle, 0 };
    dl_iterate_phdr(DlCallback, &ctx);
    return ctx.baseAddress;
}

export struct ModuleSegment {
    std::uintptr_t start = 0;
    std::size_t size = 0;
};

struct SegmentContext {
    std::string_view targetName;
    std::vector<ModuleSegment> execSegments;
    std::vector<ModuleSegment> readSegments;
};

int DlSegmentCallback(struct dl_phdr_info* info, size_t size, void* data) {
    auto* ctx = reinterpret_cast<SegmentContext*>(data);
    const char* filename = GetFilename(info->dlpi_name);

    if (ctx->targetName == filename) {
        for (int i = 0; i < info->dlpi_phnum; i++) {
            const ElfW(Phdr)* phdr = &info->dlpi_phdr[i];
            if (phdr->p_type == PT_LOAD) {
                ModuleSegment seg;
                seg.start = info->dlpi_addr + phdr->p_vaddr;
                seg.size = phdr->p_memsz;
                if (phdr->p_flags & PF_X)
                    ctx->execSegments.push_back(seg);
                if (phdr->p_flags & PF_R)
                    ctx->readSegments.push_back(seg);
            }
        }
        return 1;
    }
    return 0;
}

export std::vector<ModuleSegment> FindExecutableSegments(const std::string_view needle) {
    SegmentContext ctx{ needle, {}, {} };
    dl_iterate_phdr(DlSegmentCallback, &ctx);
    return ctx.execSegments;
}

// targetFn adresinin gerçekten modülün bellek sayfalarından birine ait olup olmadığı
export bool IsAddressInModule(const std::string_view needle, std::uintptr_t address) {
    SegmentContext ctx{ needle, {}, {} };
    dl_iterate_phdr(DlSegmentCallback, &ctx);
    for (const auto& seg : ctx.readSegments)
        if (address >= seg.start && address < seg.start + seg.size)
            return true;
    for (const auto& seg : ctx.execSegments)
        if (address >= seg.start && address < seg.start + seg.size)
            return true;
    return false;
}

// Basit AOB (array-of-bytes) tarayıcı. `pattern` boşlukla ayrılmış token'lardan
// oluşur: "48 89 5C 24 ? 56 ..." — '?' veya "??" wildcard. needle modülünün
// exec segmentlerinde tarar, bulunan adresi (mutlak) döndürür, yoksa 0.
export std::uintptr_t PatternScan(const std::string_view needle, const std::string_view pattern) {
    const auto base = FindModuleBase(needle);
    if (!base)
        return 0;

    // İmzayı parse et: bytes + wildcard maskesi
    std::vector<std::uint8_t> sig;
    std::vector<bool> wild;
    std::size_t i = 0;
    while (i < pattern.size()) {
        // boşlukları atla
        while (i < pattern.size() && pattern[i] == ' ')
            ++i;
        if (i >= pattern.size())
            break;
        // bir token al (boşluğa kadar)
        std::size_t j = i;
        while (j < pattern.size() && pattern[j] != ' ')
            ++j;
        std::string_view tok = pattern.substr(i, j - i);
        i = j;

        if (tok == "?" || tok == "??") {
            sig.push_back(0);
            wild.push_back(true);
        } else {
            auto toHex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            if (tok.size() != 2)
                return 0;
            int hi = toHex(tok[0]);
            int lo = toHex(tok[1]);
            if (hi < 0 || lo < 0)
                return 0;
            sig.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
            wild.push_back(false);
        }
    }
    if (sig.empty())
        return 0;

    auto segs = FindExecutableSegments(needle);
    for (const auto& seg : segs) {
        const auto start = seg.start;
        const auto size = seg.size;
        if (size < sig.size())
            continue;
        const auto* mem = reinterpret_cast<const std::uint8_t*>(start);
        for (std::size_t k = 0; k + sig.size() <= size; ++k) {
            bool ok = true;
            for (std::size_t m = 0; m < sig.size(); ++m) {
                if (!wild[m] && mem[k + m] != sig[m]) {
                    ok = false;
                    break;
                }
            }
            if (ok)
                return start + k;
        }
    }
    return 0;
}
