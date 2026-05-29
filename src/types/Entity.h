#pragma once
#include "UnityTypes.h"
#include "enums/EntityType.h"

class Entity : public Il2CppObject {
public:
    Entity(uintptr_t addr) : Il2CppObject(addr) {}

    Field<int64_t, 0x88> id{this};

    //note EntityLook 8553 for portals
    Field<EntityType, 0x60> type{this};
    Field<int32_t, 0x50, 0x84> cellId{this};
    Field<Il2CppString, 0x38, 0xD8, 0x20> animationState{this};
};