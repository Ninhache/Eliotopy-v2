#pragma once
#include "UnityTypes.h"
#include "LocalPlayerService.h"

class DataService : public Il2CppObject {
public:
    DataService(uintptr_t addr) : Il2CppObject(addr) {}

    Field<LocalPlayerService, 0x38> localPlayerService{this};
};
