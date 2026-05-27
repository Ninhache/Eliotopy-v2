#pragma once
#include "UnityTypes.h"
#include "ClientMapData.h"

class MapMetadata : public ScriptableObject {
public:
    MapMetadata(uintptr_t addr) : ScriptableObject(addr) {}

    Field<ClientMapData, 0x18> mapData{this};
};