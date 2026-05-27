#pragma once
#include "UnityTypes.h"
#include "LocalPlayerSummary.h"
#include "LocalPlayerHud.h"
#include "Entity.h"
#include "FightService.h"
#include "Glyph.h"
#include "memory/unity/ManagedMap.h"

class LocalPlayerService : public Il2CppObject {
public:
    LocalPlayerService(uintptr_t addr) : Il2CppObject(addr) {}

    Field<LocalPlayerSummary, 0x128> playerSummary{this};
    Field<ManagedMap<int, LocalPlayerHud>, 0x70, 0x18, 0x40> playerHuds{this};
    
    Field<ManagedMap<int64_t, Entity>, 0x28, 0x18, 0xC8> entities{this};

    Field<bool, 0x70, 0x18, 0x1A0, 0x98> isInFight{this};
    Field<ManagedMap<int64_t, int32_t>, 0x70, 0x18, 0x170, 0x140> teamByFighterId{this};
    Field<FightService, 0x30, 0x80> fightService{this};
    Field<ManagedMap<int32_t, Glyph>, 0x30, 0xF0, 0x80> glyphs{this};
};
