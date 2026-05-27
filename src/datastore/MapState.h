#pragma once
#include <cstdint>
#include "json.hpp"

struct Cell {
    int32_t cellNumber = 0;
    int32_t speed = 0;
    int32_t mapChangeData = 0;
    int32_t moveZone = 0;
    int32_t linkedZone = 0;
    bool mov = false;
    bool los = false;
    bool nonWalkableDuringFight = false;
    bool nonWalkableDuringRP = false;
    bool farmCell = false;
    bool visible = false;
    bool havenbagCell = false;
    bool roleplayMonstersMovementBlocked = false;

    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool operator==(const Cell& other) const {
        return cellNumber == other.cellNumber && speed == other.speed && mapChangeData == other.mapChangeData && moveZone == other.moveZone && linkedZone == other.linkedZone && mov == other.mov && los == other.los && nonWalkableDuringFight == other.nonWalkableDuringFight && nonWalkableDuringRP == other.nonWalkableDuringRP && farmCell == other.farmCell && visible == other.visible && havenbagCell == other.havenbagCell && roleplayMonstersMovementBlocked == other.roleplayMonstersMovementBlocked && x == other.x && y == other.y && width == other.width && height == other.height;
    }

    bool operator!=(const Cell& other) const {
        return !(*this == other);
    }
};

struct MapInfo {
    int32_t flags = 0;
    uint32_t id = 0;
    int16_t posX = 0;
    int16_t posY = 0;
    int32_t nameId = 0;
    uint16_t subAreaId = 0;
    int8_t worldMap = 0;
};

inline void to_json(nlohmann::json& j, const MapInfo& m) {
    j = nlohmann::json{
        {"flags", m.flags},
        {"id", m.id},
        {"posX", m.posX},
        {"posY", m.posY},
        {"nameId", m.nameId},
        {"subAreaId", m.subAreaId},
        {"worldMap", m.worldMap}
    };
}
