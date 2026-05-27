#pragma once
#include <vector>
#include "PlayerState.h"
#include "EntityState.h"
#include "PortalState.h"
#include "glm/glm.hpp"
#include "MapState.h"
#include <unordered_map>

struct GameState {
    bool isInGame = false;
    bool isInFight = false;
    int64_t currentTurnEntityId = 0;
    bool isPlayerTurn = false;
    int64_t trackedEntityId = 0;

    PlayerState player;
    
    std::unordered_map<int64_t, EntityState> mapEntities;
    std::unordered_map<int64_t, EntityState> fightEntities;
    std::unordered_map<int64_t, PortalState> portalEntities;

    std::vector<Cell> cells;
    glm::mat4 viewProjMatrix = glm::mat4(1.0f);
    
    MapInfo mapInfo;
};

inline void to_json(nlohmann::json& j, const GameState& g) {
    auto mapEntitiesJson = nlohmann::json::array();
    for (const auto& [id, entity] : g.mapEntities) mapEntitiesJson.push_back(entity);

    auto fightEntitiesJson = nlohmann::json::array();
    for (const auto& [id, entity] : g.fightEntities) fightEntitiesJson.push_back(entity);

    auto portalsJson = nlohmann::json::array();
    for (const auto& [id, portal] : g.portalEntities) portalsJson.push_back(portal);

    j = nlohmann::json{
        {"isInGame", g.isInGame},
        {"isInFight", g.isInFight},
        {"currentTurnEntityId", g.currentTurnEntityId},
        {"isPlayerTurn", g.isPlayerTurn},
        {"player", g.player},
        {"mapEntities", mapEntitiesJson},
        {"fightEntities", fightEntitiesJson},
        {"portals", portalsJson},
        {"mapInfo", g.mapInfo},
        {"trackedEntityId", g.trackedEntityId}
    };
}
