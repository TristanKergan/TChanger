module;
#include <cstdint>
#include <cstdio>

import Logger;
import Modules;
import Offsets;
import Memory;
import Resolver;
import SchemaScan;
import Config;
import ItemCatalog;

export module AgentChanger;

// Minimal ajan değiştirici: FSN stage 7'de (FSN hook güvenli penceresi) sürülür.
// Local player controller -> m_hPawn -> pawn zincirini yürür, takıma göre config'den
// gelen ajan index'ini ItemCatalog tablosundan alır ve C_BaseModelEntity::SetModel
// ile model yolunu uygular. Offset'ler dinamik çözülür (Resolver).

namespace {

const char* AgentModelForDef(std::uint32_t def) {
    const auto* a = itemcatalog::FindAgentByDef(def);
    return a ? a->modelPath : nullptr;
}

}  // namespace

export class AgentChanger {
public:
    static AgentChanger& Global();
    void Run();  // FSN stage 7: ajan modelini SetModel ile uygula

private:
    void* lastPawn_ = nullptr;
    std::uint8_t lastTeam_ = 0;
    std::uint32_t lastDef_ = 0;
};

AgentChanger& AgentChanger::Global() {
    static AgentChanger instance;
    return instance;
}

void AgentChanger::Run() {
    static Logger log;
    static bool dBase = false, dCtrl = false, dPawnH = false, dSys = false,
                 dPawn = false, dTeam = false, dModel = false;

    const auto& o = ResolveClientOffsets();
    if (!o.resolved || !o.localPlayerController || !o.entitySystem || !o.setModel) {
        if (!dBase) { log.error("agent: offsets not resolved"); dBase = true; }
        return;
    }

    void* controller = TryController(o.localPlayerController, ClientModule);
    if (!controller) {
        if (!dCtrl) { log.warn("agent: controller null/bad"); dCtrl = true; }
        return;
    }

    if (IsBadReadPtr(reinterpret_cast<const std::uint8_t*>(controller) + o.m_hPawn,
                     sizeof(std::uint32_t))) {
        if (!dPawnH) { log.warn("agent: pawnHandle read bad"); dPawnH = true; }
        return;
    }
    const auto pawnHandle = *reinterpret_cast<const std::uint32_t*>(
        reinterpret_cast<const std::uint8_t*>(controller) + o.m_hPawn);

    const auto system = ReadPointer(o.entitySystem);
    if (!system || IsBadReadPtr(reinterpret_cast<const void*>(system), sizeof(void*))) {
        if (!dSys) { log.warn("agent: entitySystem null/bad"); dSys = true; }
        return;
    }

    void* const pawn = EntityFromHandle(system, o.entityList, pawnHandle);
    if (!pawn || IsBadReadPtr(pawn, 8)) {
        if (!dPawn) { log.warn("agent: pawn null/bad"); dPawn = true; }
        return;
    }

    const auto& so = ResolveSkin();
    if (so.team_num_offset == 0) {
        if (!dTeam) { log.warn("agent: m_iTeamNum offset unresolved"); dTeam = true; }
        return;
    }

    if (IsBadReadPtr(reinterpret_cast<const std::uint8_t*>(pawn) + so.team_num_offset,
                     sizeof(std::uint8_t)))
        return;

    const auto team = *reinterpret_cast<const std::uint8_t*>(
        reinterpret_cast<const std::uint8_t*>(pawn) + so.team_num_offset);

    // config'ten ajan def index'i (doğrudan ekonomi item def index'i).
    const auto& cfg = config::Global();
    if (!config::ConfigStore::Instance().isAgentEnabled()) {
        if (!dTeam) { log.warn("agent: agentEnabled=false, skip"); dTeam = true; }
        return;
    }

    if (team != 3 && team != 2) {
        if (!dTeam) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "agent: team not 2/3 (=%u) -> spectator, skip",
                          static_cast<unsigned>(team));
            log.warn(buf);
            dTeam = true;
        }
        return;  // spectator/unassigned: pawn'a dokunma
    }

    const bool isCT = (team == 3);
    const std::uint32_t def = isCT ? cfg.ctAgentDef : cfg.tAgentDef;
    const char* const model = AgentModelForDef(def);
    if (!model) {
        if (!dModel) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "agent: def %u icin model bulunamadi", def);
            log.warn(buf);
            dModel = true;
        }
        return;
    }

    // Aynı pawn/takım/ajan zaten uygulandıysa atla.
    if (pawn == lastPawn_ && team == lastTeam_ && def == lastDef_)
        return;

    using SetModelFn = void (*)(void*, const char*);
    reinterpret_cast<SetModelFn>(o.setModel)(pawn, model);

    char buf[160];
    std::snprintf(buf, sizeof(buf), "agent: SetModel(%s) on pawn %p (team %u, def %u)",
                  model, pawn, static_cast<unsigned>(team), def);
    log.info(buf);
    lastPawn_ = pawn;
    lastTeam_ = team;
    lastDef_ = def;
}
