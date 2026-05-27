#pragma once
#include "UnityTypes.h"

class LocalPlayerHud : public Il2CppObject {
public:
    LocalPlayerHud(uintptr_t addr) : Il2CppObject(addr) {}

    Field<int32_t,  0x214> hp{this};
    Field<int32_t,  0x218> hpMax{this};
    Field<uint32_t, 0x21C> hpPercent{this};
    Field<int32_t,  0x220> actionPoints{this};
    Field<int32_t,  0x224> movementPoints{this};
    Field<int32_t,  0x228> invocationPoints{this};
    Field<int32_t,  0x22C> shieldPoints{this};
};
