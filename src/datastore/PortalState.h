#pragma once
#include <string>
#include <cstdint>
#include "json.hpp"

enum class PortalClosedReason : int32_t {
    None = 0,
    EntityOnTop = 1,
    ClosedOrUsed = 2
};

struct PortalState {
    int64_t id = 0;
    int32_t cellId = -1;
    bool isOpen = true;
    int32_t index = 0;
    PortalClosedReason closedReason = PortalClosedReason::None;

    bool effectiveOpen() const { return closedReason == PortalClosedReason::None; }
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PortalState, id, cellId, isOpen, index, closedReason)
