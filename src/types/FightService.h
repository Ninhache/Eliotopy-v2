#pragma once
#include "UnityTypes.h"

class FightService : public Il2CppObject {
public:
    FightService(uintptr_t addr) : Il2CppObject(addr) {}

    Field<int64_t, 0x178> currentTurnEntityId{this};
};
