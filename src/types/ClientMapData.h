#pragma once
#include "UnityTypes.h"
#include "ClientCellData.h"

class ClientMapData : public Il2CppObject {
public:
    ClientMapData(uintptr_t addr) : Il2CppObject(addr) {}

    Field<Il2CppList<ClientCellData>, 0xD0> cellsList{this};
};