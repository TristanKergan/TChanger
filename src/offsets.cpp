module;
#include <cstdint>
#include <string_view>

export module Offsets;

// Modül ismi
export inline constexpr std::string_view ClientModule = "libclient.so";

// --- Build'e özel sabitler ---
// Global offset'ler (LocalPlayerControllerSlot, EntitySystemSlot, SetModelOffset,
// EntityListOffset, OffsetToBasePawnHandle) artık dinamik çözülüyor — bkz. Resolver.
// Sadece alan (field) offset'ler ve arayüz/FSN sabitleri burada kalır.
// FSN (FrameStageNotify) hook: Source2Client002 arayüzünün canlı vtable'ı.
// Önceki yaklaşım (mutlak `base + slotOffset`) stale olduğu için terk edildi;
// arayüz instance'ı CreateInterface ile alınıp onun vtable'ı hook'lanır.
export inline constexpr std::string_view ClientInterfaceName = "Source2Client002";
// Birincil FSN vtable index'i (2025+ build'lerde stabil). İmza taraması yerine
// doğrudan bu index kullanılır; slot'un hedef fonksiyonu libclient.so içinde mi
// diye FsnHook içinde doğrulanır (drift olursa güvenli şekilde hata verir).
export inline constexpr std::size_t FsnVtableIndex = 36;
export inline constexpr int FRAME_NET_UPDATE_POSTDATAUPDATE_START = 6;    // stage 6
export inline constexpr int FRAME_NET_UPDATE_POSTDATAUPDATE_END = 7;     // stage 7
