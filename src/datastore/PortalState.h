#pragma once
#include <string>
#include <cstdint>
#include "json.hpp"

struct PortalState {
    int64_t id = 0;
    int32_t cellId = -1;
    bool isOpen = true;
    int32_t index = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PortalState, id, cellId, isOpen, index)
