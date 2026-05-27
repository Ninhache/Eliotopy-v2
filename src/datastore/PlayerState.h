#pragma once
#include <string>
#include <cstdint>
#include "enums/BreedEnum.h"
#include "json.hpp"

struct PlayerState {
    std::string name;
    int64_t id = 0;
    BreedEnum breed = BreedEnum::Undefined;
    int32_t cellId = -1;

    int32_t hp = 0;
    int32_t hpMax = 0;
    uint32_t hpPercent = 0;
    int32_t ap = 0;
    int32_t mp = 0;
    int32_t invocationPoints = 0;
    int32_t shield = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerState, name, id, breed, cellId, hp, hpMax, hpPercent, ap, mp, invocationPoints, shield)
