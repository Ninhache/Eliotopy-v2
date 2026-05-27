#pragma once
#include "UnityTypes.h"
#include "MapInformationData.h"

class MapRenderer : public MonoBehaviour {
public:
    MapRenderer(uintptr_t addr) : MonoBehaviour(addr) {}

    MapRenderer() : MonoBehaviour("Map Renderer", "MapRenderer") {}

    Field<MapInformationData, 0x120> mapInformationData{this};
};
