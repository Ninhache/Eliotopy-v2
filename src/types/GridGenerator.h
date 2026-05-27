#pragma once
#include "UnityTypes.h"
#include "DataService.h"

class GridGenerator : public Il2CppObject {
public:
    GridGenerator(uintptr_t addr) : Il2CppObject(addr) {}

    Field<DataService, 0xB0> dataService{this};
};
