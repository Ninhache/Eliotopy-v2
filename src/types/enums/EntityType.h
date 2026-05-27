#pragma once
#include <cstdint>
#include <string>

enum class EntityType : int32_t {
    Player = 0x5201,
    Monster = 0x0201,
    Portal = 0x0004
};

inline const char* toString(EntityType t) {
    switch (t) {
        case EntityType::Player:  return "Player";
        case EntityType::Monster: return "Monster";
        case EntityType::Portal:  return "Portal";
        default:                  return "Unknown";
    }
}
