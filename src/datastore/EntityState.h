#pragma once
#include <string>
#include <cstdint>
#include "enums/EntityType.h"
#include "json.hpp"

struct EntityState {
    int64_t id = 0;
    EntityType type = EntityType::Player;
    int32_t cellId = -1;
    std::string animationState;
    bool isActive = false;
    int32_t teamId = -1;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EntityState, id, type, cellId, animationState, isActive, teamId)
