#pragma once
#include "UnityTypes.h"

class Glyph : public Il2CppObject {
public:
    Glyph(uintptr_t addr) : Il2CppObject(addr) {}

    Field<bool, 0x10> active{this};
    Field<int64_t, 0x40> ownerId{this};
    Field<int32_t, 0x48> globalIndex{this};
    Field<int32_t, 0x2C> glyphTypeId {this}; //44338 portal?
    Field<int32_t, 0x4C> cellId{this};
};
