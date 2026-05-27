#pragma once
#include "UnityTypes.h"
#include "MapMetadata.h"
#include "GridGenerator.h"
#include "GraphicCell.h"

class InteractiveCellManager : public MonoBehaviour {
public:
    InteractiveCellManager(uintptr_t addr) : MonoBehaviour(addr) {}

    InteractiveCellManager() : MonoBehaviour("InteractiveCellManager(Clone)", "InteractiveCellManager") {}

    Field<MapMetadata, 0xE8> mapMetadata{this};
    Field<GridGenerator, 0x28> gridGenerator{this};
    Field<Il2CppList<GraphicCell>, 0x110> graphicCells{this};
};