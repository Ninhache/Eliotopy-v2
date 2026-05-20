#pragma once
#include "UnityTypes.h"

class ClientCellData : public Il2CppObject {
public:
    ClientCellData(uintptr_t addr) : Il2CppObject(addr) {}

    Field<uint32_t, 0x10> cellNumber{this};
    Field<int32_t,  0x14> speed{this};
    Field<int32_t,  0x18> mapChangeData{this};
    Field<int32_t,  0x1C> moveZone{this};
    Field<int32_t,  0x20> linkedZone{this};
    Field<bool,     0x24> mov{this};
    Field<bool,     0x25> los{this};
    Field<bool,     0x26> nonWalkableDuringFight{this};
    Field<bool,     0x27> nonWalkableDuringRP{this};
    Field<bool,     0x28> farmCell{this};
    Field<bool,     0x29> visible{this};
    Field<bool,     0x2A> havenbagCell{this};
    Field<bool,     0x2B> roleplayMonstersMovementBlocked{this};
    Field<int32_t,  0x2C> floor{this};
    Field<bool,     0x30> red{this};
    Field<bool,     0x31> blue{this};
    Field<int32_t,  0x34> arrow{this};
};