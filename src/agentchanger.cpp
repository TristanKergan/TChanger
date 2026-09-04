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

export module AgentChanger;

// Minimal ajan değiştirici: FSN stage 7'de (FSN hook güvenli penceresi) sürülür.
// Local player controller -> m_hPawn -> pawn zincirini yürür, takıma göre config'den
// gelen ajan index'ini Agents[] tablosundan alır ve C_BaseModelEntity::SetModel
// ile model yolunu uygular. Offset'ler dinamik çözülür (Resolver).

// Ajan def index'leri artık config.json "settings" altından gelir
// (agent_ct_index / agent_t_index -> Agents[] index).

// Agent def index -> model yolu.
struct AgentModel {
    std::uint32_t def;
    const char* modelPath;
};

const AgentModel Agents[] = {
    {4619, "agents/models/ctm_st6/ctm_st6_variantj.vmdl"},
    {4680, "agents/models/ctm_st6/ctm_st6_variantl.vmdl"},
    {4711, "agents/models/ctm_swat/ctm_swat_variante.vmdl"},
    {4712, "agents/models/ctm_swat/ctm_swat_variantf.vmdl"},
    {4713, "agents/models/ctm_swat/ctm_swat_variantg.vmdl"},
    {4714, "agents/models/ctm_swat/ctm_swat_varianth.vmdl"},
    {4715, "agents/models/ctm_swat/ctm_swat_varianti.vmdl"},
    {4716, "agents/models/ctm_swat/ctm_swat_variantj.vmdl"},
    {4718, "agents/models/tm_balkan/tm_balkan_variantk.vmdl"},
    {4726, "agents/models/tm_professional/tm_professional_varf.vmdl"},
    {4727, "agents/models/tm_professional/tm_professional_varg.vmdl"},
    {4728, "agents/models/tm_professional/tm_professional_varh.vmdl"},
    {4730, "agents/models/tm_professional/tm_professional_varj.vmdl"},
    {4732, "agents/models/tm_professional/tm_professional_vari.vmdl"},
    {4733, "agents/models/tm_professional/tm_professional_varf1.vmdl"},
    {4734, "agents/models/tm_professional/tm_professional_varf2.vmdl"},
    {4735, "agents/models/tm_professional/tm_professional_varf3.vmdl"},
    {4736, "agents/models/tm_professional/tm_professional_varf4.vmdl"},
    {4613, "agents/models/tm_professional/tm_professional_varf5.vmdl"},
    {4749, "agents/models/ctm_gendarmerie/ctm_gendarmerie_varianta.vmdl"},
    {4750, "agents/models/ctm_gendarmerie/ctm_gendarmerie_variantb.vmdl"},
    {4751, "agents/models/ctm_gendarmerie/ctm_gendarmerie_variantc.vmdl"},
    {4752, "agents/models/ctm_gendarmerie/ctm_gendarmerie_variantd.vmdl"},
    {4753, "agents/models/ctm_gendarmerie/ctm_gendarmerie_variante.vmdl"},
    {4756, "agents/models/ctm_swat/ctm_swat_variantk.vmdl"},
    {4757, "agents/models/ctm_diver/ctm_diver_varianta.vmdl"},
    {4771, "agents/models/ctm_diver/ctm_diver_variantb.vmdl"},
    {4772, "agents/models/ctm_diver/ctm_diver_variantc.vmdl"},
    {4773, "agents/models/tm_jungle_raider/tm_jungle_raider_varianta.vmdl"},
    {4774, "agents/models/tm_jungle_raider/tm_jungle_raider_variantb.vmdl"},
    {4775, "agents/models/tm_jungle_raider/tm_jungle_raider_variantc.vmdl"},
    {4776, "agents/models/tm_jungle_raider/tm_jungle_raider_variantd.vmdl"},
    {4777, "agents/models/tm_jungle_raider/tm_jungle_raider_variante.vmdl"},
    {4778, "agents/models/tm_jungle_raider/tm_jungle_raider_variantf.vmdl"},
    {4780, "agents/models/tm_jungle_raider/tm_jungle_raider_variantb2.vmdl"},
    {4781, "agents/models/tm_jungle_raider/tm_jungle_raider_variantf2.vmdl"},
    {5105, "agents/models/tm_leet/tm_leet_variantg.vmdl"},
    {5106, "agents/models/tm_leet/tm_leet_varianth.vmdl"},
    {5107, "agents/models/tm_leet/tm_leet_varianti.vmdl"},
    {5108, "agents/models/tm_leet/tm_leet_variantf.vmdl"},
    {5109, "agents/models/tm_leet/tm_leet_variantj.vmdl"},
    {5205, "agents/models/tm_phoenix/tm_phoenix_varianth.vmdl"},
    {5206, "agents/models/tm_phoenix/tm_phoenix_variantf.vmdl"},
    {5207, "agents/models/tm_phoenix/tm_phoenix_variantg.vmdl"},
    {5208, "agents/models/tm_phoenix/tm_phoenix_varianti.vmdl"},
    {5305, "agents/models/ctm_fbi/ctm_fbi_variantf.vmdl"},
    {5306, "agents/models/ctm_fbi/ctm_fbi_variantg.vmdl"},
    {5307, "agents/models/ctm_fbi/ctm_fbi_varianth.vmdl"},
    {5308, "agents/models/ctm_fbi/ctm_fbi_variantb.vmdl"},
    {5400, "agents/models/ctm_st6/ctm_st6_variantk.vmdl"},
    {5401, "agents/models/ctm_st6/ctm_st6_variante.vmdl"},
    {5402, "agents/models/ctm_st6/ctm_st6_variantg.vmdl"},
    {5403, "agents/models/ctm_st6/ctm_st6_variantm.vmdl"},
    {5404, "agents/models/ctm_st6/ctm_st6_varianti.vmdl"},
    {5405, "agents/models/ctm_st6/ctm_st6_variantn.vmdl"},
    {5500, "agents/models/tm_balkan/tm_balkan_variantf.vmdl"},
    {5501, "agents/models/tm_balkan/tm_balkan_varianti.vmdl"},
    {5502, "agents/models/tm_balkan/tm_balkan_variantg.vmdl"},
    {5503, "agents/models/tm_balkan/tm_balkan_variantj.vmdl"},
    {5504, "agents/models/tm_balkan/tm_balkan_varianth.vmdl"},
    {5505, "agents/models/tm_balkan/tm_balkan_variantl.vmdl"},
    {5601, "agents/models/ctm_sas/ctm_sas_variantf.vmdl"},
    {5602, "agents/models/ctm_sas/ctm_sas_variantg.vmdl"},
};

const char* AgentModelForDef(std::uint32_t def) {
    for (const auto& agent : Agents) {
        if (agent.def == def)
            return agent.modelPath;
    }
    return nullptr;
}

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

    // Controller: rip-relative +1 quirk — önce canonical, değilse +1 dene.
    void* controller = TryController(o.localPlayerController, ClientModule);
    if (!controller)
        controller = TryController(o.localPlayerController + 1, ClientModule);
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
