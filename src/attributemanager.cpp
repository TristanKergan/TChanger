module;
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

export module AttributeManager;

import Logger;
import Memory;

// In-process port of the FemboyChanger AttributeManager (Core/AttributeManager.cs):
// installs CEconItemAttribute entries (paint kit / seed / wear) on a C_EconItemView
// so the econ layer actually renders the configured skin. The fallback fields on
// C_EconEntity alone do not produce a custom material for knives, so the real
// attribute list must be populated for the paint kit to resolve.
//
// The item view's attribute list is a CUtlVectorEmbeddedNetworkVar<CEconItemAttribute>:
//   +0x00 int   m_Size
//   +0x08 T*    m_pElements
//   +0x14 int   m_nAllocationCount / flags
// The allocation-count dword carries the "external const buffer" marker
// (0xC0000000): CAttributeList's destructor only hands m_pElements to the game
// allocator when (allocCount & 0xC0000000) == 0, so setting those bits stops the
// game from freeing a block we allocated with its own allocator.

namespace {

// CEconItemAttribute, laid out for direct field writes. The layout comes from the
// schema registration in client.dll (size 0x48): m_iAttributeDefinitionIndex@0x30,
// m_flValue@0x34, m_flInitialValue@0x38, m_nRefundableCurrency@0x3C, m_bSetBonus@0x40.
struct EconItemAttribute {
    void* vtable;
    void* owner;
    std::uint8_t pad0[0x30 - 0x10];
    std::uint16_t defIndex;  // 0x30
    float value;             // 0x34
    float initialValue;      // 0x38
    std::int32_t refundableCurrency;  // 0x3C
    std::uint8_t setBonus;   // 0x40
    std::uint8_t pad1[7];
};

std::unordered_map<void*, void*> sAllocatedBlocks;
std::mutex sAllocMtx;

void TrackAllocation(void* head, void* newBlock) {
    std::lock_guard<std::mutex> lk(sAllocMtx);
    auto it = sAllocatedBlocks.find(head);
    if (it != sAllocatedBlocks.end()) {
        it->second = newBlock;
    } else {
        sAllocatedBlocks.emplace(head, newBlock);
    }
}

}  // namespace

export void ClearAllocations() {
    std::lock_guard<std::mutex> lk(sAllocMtx);
    // UAF Önleme / Mimari Kısıt:
    // CS2 çalışırken veya unload sırasında sAllocatedBlocks içindeki blokları
    // std::free() ile serbest bırakmak garantili Use-After-Free (UAF) ve crash'e yol açar;
    // çünkü motorun C_EconItemView nesneleri hala m_pElements üzerinden bu belleğe işaret eder.
    // Hamzex entity destructor hook'una sahip olmadığından, UAF yerine kontrollü sızıntı
    // (silah başına ~216 byte) tercih edilir.
    sAllocatedBlocks.clear();
}

export class AttributeManager {
public:
    static constexpr std::size_t kAttributeSize = 72;  // sizeof(CEconItemAttribute)

    // Econ attribute definition indices (FemboyChanger ATTR_*).
    static constexpr std::uint16_t kAttrPaintKit = 6;  // "set item texture prefab"
    static constexpr std::uint16_t kAttrSeed = 7;      // "set item texture seed"
    static constexpr std::uint16_t kAttrWear = 8;      // "set item texture wear"

    // Byte offsets inside a CEconItemAttribute.
    static constexpr std::size_t kDefIndexOffset = 0x30;
    static constexpr std::size_t kValueOffset = 0x34;
    static constexpr std::size_t kInitialValueOffset = 0x38;

    // CUtlVector head offsets.
    static constexpr std::size_t kSizeOffset = 0x00;
    static constexpr std::size_t kElementsOffset = 0x08;
    static constexpr std::size_t kAllocCountOffset = 0x14;

    // "External const buffer" marker: stops the game freeing our allocation.
    static constexpr std::uint32_t kExternalConstBufferMarker = 0xC0000000u;

    // Records the vtable of a game-owned attribute, if this item view happens to
    // have one. Cheap enough to call on every weapon we walk past.
    static void ObserveVTable(void* itemView, int attributeListOffset, int attributesOffset);

    // Puts the skin's paint kit / seed / wear on an item view, whether or not it
    // already has an attribute list. Returns true on success.
    static bool Apply(void* itemView, int attributeListOffset, int attributesOffset,
                      std::int32_t paintKit, std::int32_t seed, float wear);

    // True once a live attribute vtable has been captured.
    static bool HasVTable() noexcept { return sVTable != nullptr; }

private:
    // Rewrites the values of existing paint-kit / seed / wear attributes in a list.
    static bool UpdateInPlace(void* head, std::int32_t paintKit, std::int32_t seed, float wear);
    // Appends paint-kit / seed / wear attributes to an existing list that lacks them.
    static bool Append(void* head, std::int32_t paintKit, std::int32_t seed, float wear);
    // Installs paint-kit / seed / wear attributes on a list that is currently empty.
    static bool Create(void* head, std::int32_t paintKit, std::int32_t seed, float wear);
    static void* ListAddress(void* itemView, int attributeListOffset, int attributesOffset);

    static void* sVTable;
};

// sVTable + dummy to drive the static_assert against kAttributeSize.
namespace { constexpr std::size_t AttributeManager_ExportHelper = 72; }
static_assert(sizeof(EconItemAttribute) == 72, "CEconItemAttribute size must be 0x48");

void* AttributeManager::sVTable = nullptr;

void* AttributeManager::ListAddress(void* itemView, int attributeListOffset, int attributesOffset) {
    return reinterpret_cast<std::uint8_t*>(itemView) + attributeListOffset + attributesOffset;
}

void AttributeManager::ObserveVTable(void* itemView, int attributeListOffset, int attributesOffset) {
    if (sVTable)
        return;
    if (attributeListOffset == 0 || attributesOffset == 0)
        return;

    void* const head = ListAddress(itemView, attributeListOffset, attributesOffset);
    if (IsBadReadPtr(head, kAllocCountOffset + sizeof(std::uint32_t)))
        return;

    const std::int32_t size = *reinterpret_cast<const std::int32_t*>(
        reinterpret_cast<const std::uint8_t*>(head) + kSizeOffset);
    void* const elements = *reinterpret_cast<void* const*>(
        reinterpret_cast<const std::uint8_t*>(head) + kElementsOffset);
    if (size <= 0 || size > 64 || !elements || IsBadReadPtr(elements, sizeof(void*)))
        return;

    void* const vtable = *reinterpret_cast<void* const*>(elements);
    if (vtable) {
        sVTable = vtable;
        static Logger log;
        log.info("AttributeManager: captured CEconItemAttribute vtable");
    }
}

bool AttributeManager::Apply(void* itemView, int attributeListOffset, int attributesOffset,
                             std::int32_t paintKit, std::int32_t seed, float wear) {
    if (paintKit <= 0)
        return false;
    if (attributeListOffset == 0 || attributesOffset == 0)
        return false;

    void* const head = ListAddress(itemView, attributeListOffset, attributesOffset);
    const std::int32_t size = *reinterpret_cast<const std::int32_t*>(
        reinterpret_cast<const std::uint8_t*>(head) + kSizeOffset);
    void* const elements = *reinterpret_cast<void* const*>(
        reinterpret_cast<const std::uint8_t*>(head) + kElementsOffset);

    if (size > 0 && elements) {
        if (UpdateInPlace(head, paintKit, seed, wear))
            return true;
        // Attribute list exists but paint kit attribute is missing: append safely
        return Append(head, paintKit, seed, wear);
    }
    return Create(head, paintKit, seed, wear);
}

bool AttributeManager::UpdateInPlace(void* head, std::int32_t paintKit, std::int32_t seed, float wear) {
    const std::int32_t size = *reinterpret_cast<const std::int32_t*>(
        reinterpret_cast<const std::uint8_t*>(head) + kSizeOffset);
    if (size <= 0 || size > 64)
        return false;
    void* const elements = *reinterpret_cast<void* const*>(
        reinterpret_cast<const std::uint8_t*>(head) + kElementsOffset);
    if (!elements)
        return false;

    bool wrotePaintKit = false;
    for (std::int32_t i = 0; i < size; ++i) {
        auto* const entry = reinterpret_cast<std::uint8_t*>(elements) + i * kAttributeSize;
        const std::uint16_t defIndex = *reinterpret_cast<const std::uint16_t*>(
            entry + kDefIndexOffset);

        float val = 0.0f;
        switch (defIndex) {
            case kAttrPaintKit:
                val = std::bit_cast<float>(paintKit);
                wrotePaintKit = true;
                break;
            case kAttrSeed:
                val = std::bit_cast<float>(seed);
                break;
            case kAttrWear:
                val = wear;
                break;
            default:
                continue;
        }

        *reinterpret_cast<float*>(entry + kValueOffset) = val;
        *reinterpret_cast<float*>(entry + kInitialValueOffset) = val;
    }
    return wrotePaintKit;
}

bool AttributeManager::Append(void* head, std::int32_t paintKit, std::int32_t seed, float wear) {
    if (!sVTable)
        return false;

    const std::int32_t oldSize = *reinterpret_cast<const std::int32_t*>(
        reinterpret_cast<const std::uint8_t*>(head) + kSizeOffset);
    void* const oldElements = *reinterpret_cast<void* const*>(
        reinterpret_cast<const std::uint8_t*>(head) + kElementsOffset);
    if (oldSize <= 0 || oldSize > 64 || !oldElements)
        return false;

    const std::int32_t newSize = oldSize + 3;
    void* const block = std::calloc(static_cast<std::size_t>(newSize), kAttributeSize);
    if (!block)
        return false;

    // Copy existing attributes
    std::memcpy(block, oldElements, static_cast<std::size_t>(oldSize) * kAttributeSize);

    // Append paintKit, seed, wear
    auto* const attrs = reinterpret_cast<EconItemAttribute*>(block);
    for (std::int32_t i = 0; i < 3; ++i) {
        const std::int32_t idx = oldSize + i;
        attrs[idx].vtable = sVTable;
        attrs[idx].owner = nullptr;
        attrs[idx].defIndex = i == 0 ? kAttrPaintKit : (i == 1 ? kAttrSeed : kAttrWear);
        attrs[idx].value = i == 2 ? wear : (i == 0 ? std::bit_cast<float>(paintKit)
                                                   : std::bit_cast<float>(seed));
        attrs[idx].initialValue = attrs[idx].value;
        attrs[idx].refundableCurrency = 0;
        attrs[idx].setBonus = 0;
    }

    TrackAllocation(head, block);

    auto* const bytes = reinterpret_cast<std::uint8_t*>(head);
    *reinterpret_cast<void**>(bytes + kElementsOffset) = block;
    *reinterpret_cast<std::uint32_t*>(bytes + kAllocCountOffset) = kExternalConstBufferMarker;
    *reinterpret_cast<std::int32_t*>(bytes + kSizeOffset) = newSize;
    return true;
}

bool AttributeManager::Create(void* head, std::int32_t paintKit, std::int32_t seed, float wear) {
    if (!sVTable)
        return false;

    // Never stomp a list the game already owns.
    const std::int32_t size = *reinterpret_cast<const std::int32_t*>(
        reinterpret_cast<const std::uint8_t*>(head) + kSizeOffset);
    void* const elements = *reinterpret_cast<void* const*>(
        reinterpret_cast<const std::uint8_t*>(head) + kElementsOffset);
    if (size != 0 || elements != 0)
        return false;

    void* const block = std::calloc(3, kAttributeSize);
    if (!block)
        return false;

    auto* attrs = reinterpret_cast<EconItemAttribute*>(block);
    for (std::int32_t i = 0; i < 3; ++i) {
        attrs[i].vtable = sVTable;
        attrs[i].owner = nullptr;
        attrs[i].defIndex = i == 0 ? kAttrPaintKit : (i == 1 ? kAttrSeed : kAttrWear);
        attrs[i].value = i == 2 ? wear : (i == 0 ? std::bit_cast<float>(paintKit)
                                                  : std::bit_cast<float>(seed));
        attrs[i].initialValue = attrs[i].value;
        attrs[i].refundableCurrency = 0;
        attrs[i].setBonus = 0;
    }

    TrackAllocation(head, block);

    auto* const bytes = reinterpret_cast<std::uint8_t*>(head);
    // Publish the pointer before the count so the game can never observe a
    // non-zero size pointing at a stale/garbage array.
    *reinterpret_cast<void**>(bytes + kElementsOffset) = block;
    *reinterpret_cast<std::uint32_t*>(bytes + kAllocCountOffset) = kExternalConstBufferMarker;
    *reinterpret_cast<std::int32_t*>(bytes + kSizeOffset) = 3;
    return true;
}
