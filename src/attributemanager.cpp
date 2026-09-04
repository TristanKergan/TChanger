module;
#include <cstdint>
#include <cstdlib>

export module AttributeManager;

import Logger;

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
// game from freeing a block we allocated (calloc) with its own allocator.

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

}  // namespace

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
    const std::int32_t size = *reinterpret_cast<const std::int32_t*>(
        reinterpret_cast<const std::uint8_t*>(head) + kSizeOffset);
    void* const elements = *reinterpret_cast<void* const*>(
        reinterpret_cast<const std::uint8_t*>(head) + kElementsOffset);
    if (size <= 0 || size > 64 || !elements)
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

    if (size > 0 && elements)
        return UpdateInPlace(head, paintKit, seed, wear);
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

        const float* value = nullptr;
        switch (defIndex) {
            case kAttrPaintKit:
                value = reinterpret_cast<const float*>(&paintKit);
                break;
            case kAttrSeed:
                value = reinterpret_cast<const float*>(&seed);
                break;
            case kAttrWear:
                value = &wear;
                break;
            default:
                continue;
        }
        if (!value)
            continue;

        *reinterpret_cast<float*>(entry + kValueOffset) = *value;
        *reinterpret_cast<float*>(entry + kInitialValueOffset) = *value;
        if (defIndex == kAttrPaintKit)
            wrotePaintKit = true;
    }
    return wrotePaintKit;
}

bool AttributeManager::Create(void* head, std::int32_t paintKit, std::int32_t seed, float wear) {
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
        attrs[i].value = i == 2 ? wear : (i == 0 ? *reinterpret_cast<const float*>(&paintKit)
                                                  : *reinterpret_cast<const float*>(&seed));
        attrs[i].initialValue = attrs[i].value;
        attrs[i].refundableCurrency = 0;
        attrs[i].setBonus = 0;
    }

    auto* const bytes = reinterpret_cast<std::uint8_t*>(head);
    // Publish the pointer before the count so the game can never observe a
    // non-zero size pointing at a stale/garbage array.
    *reinterpret_cast<void**>(bytes + kElementsOffset) = block;
    *reinterpret_cast<std::uint32_t*>(bytes + kAllocCountOffset) = kExternalConstBufferMarker;
    *reinterpret_cast<std::int32_t*>(bytes + kSizeOffset) = 3;
    return true;
}
