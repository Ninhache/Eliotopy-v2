#pragma once
#include "UnityTypes.h"
#include "MapMetadata.h"

class InteractiveCellManager : public MonoBehaviour {
public:
    InteractiveCellManager(uintptr_t addr) : MonoBehaviour(addr) {}

    InteractiveCellManager() : MonoBehaviour("InteractiveCellManager(Clone)", "InteractiveCellManager") {}

    RemotePtr<MapMetadata, 0xE8> mapMetadata{this};
};