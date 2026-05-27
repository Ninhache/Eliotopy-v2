#pragma once
#include "UnityTypes.h"

class GraphicCell : public Il2CppObject {
public:
    GraphicCell(uintptr_t addr) : Il2CppObject(addr) {}

    Field<float, 0x20> x{this};
    Field<float, 0x24> y{this};
    Field<float, 0x30> width{this};
    Field<float, 0x34> height{this};
};
