#pragma once
#include "Memory.h"
#include "UnityTypes.h"
#include "enums/BreedEnum.h"

class LocalPlayerSummary : public Il2CppObject {
public:
    LocalPlayerSummary(uintptr_t addr) : Il2CppObject(addr) {}

    Field<Il2CppString, 0x18> characterName{this};
    Field<int64_t,   0x10> characterId{this};
    Field<BreedEnum, 0x30> classId{this};
};
