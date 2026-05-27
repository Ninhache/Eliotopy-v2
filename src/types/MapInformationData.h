#pragma once
#include "UnityTypes.h"

class MapInformationData : public Il2CppObject {
public:
    MapInformationData(uintptr_t addr) : Il2CppObject(addr) {}

    Field<int32_t, 0x10> flags{this};
    Field<uint32_t, 0x14> id{this};
    Field<int16_t, 0x18> posX{this};
    Field<int16_t, 0x1A> posY{this};
    Field<int32_t, 0x1C> nameId{this};
    Field<uint16_t, 0x20> subAreaId{this};
    Field<int8_t, 0x22> worldMap{this};
};
