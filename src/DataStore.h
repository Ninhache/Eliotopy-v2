#pragma once
#include "InteractiveCellManager.h"
#include "TimeElapsedScopeLogger.h"
#include "datastore/GameState.h"
#include <sstream>
#include <algorithm>
#include "types/MapRenderer.h"
#include "shared/Config.h"

class DataStore {
private:
    InteractiveCellManager interactiveCellManager;
    MapRenderer mapRenderer;
    uint32_t _lastMapId = 0;
    bool _lastIsInFight = false;
    int _cellRefreshRetries = 0;

public:
    GameState states;

    void refresh() {
        TimeElapsedScopeLogger scope("Refresh datastore");
        std::vector<Cell> oldCells = std::move(states.cells);
        states = GameState{}; // reset

        if (!interactiveCellManager.load()) {
            return;
        }

        if (mapRenderer.load()) {
            auto minfo = mapRenderer.mapInformationData.get();
            if (minfo.isValid()) {
                states.mapInfo.flags = minfo.flags.get();
                states.mapInfo.id = minfo.id.get();
                states.mapInfo.posX = minfo.posX.get();
                states.mapInfo.posY = minfo.posY.get();
                states.mapInfo.nameId = minfo.nameId.get();
                states.mapInfo.subAreaId = minfo.subAreaId.get();
                states.mapInfo.worldMap = minfo.worldMap.get();
            }
        }

        auto lps = interactiveCellManager.gridGenerator.get()
            .dataService.get()
            .localPlayerService.get();

        states.isInFight = lps.isInFight.get();

        bool fightStateChanged = (states.isInFight != _lastIsInFight);
        bool needsCellRefresh = (states.mapInfo.id != _lastMapId) || fightStateChanged || oldCells.empty();

        if (needsCellRefresh) {
            auto mapMetadata = interactiveCellManager.mapMetadata.get();
            auto mapData = mapMetadata.mapData.get();
            auto cellList = mapData.cellsList.get();

            if (cellList.isValid()) {
                auto rawMapCells = cellList.getAll();
                states.isInGame = true;
                
                auto graphicCellList = interactiveCellManager.graphicCells.get();
                if (graphicCellList.isValid()) {
                    auto rawGraphicCells = graphicCellList.getAll();
                    states.cells.reserve(rawGraphicCells.size());
                    for (size_t i = 0; i < rawGraphicCells.size(); ++i) {
                        Cell c;
                        const auto& gc = rawGraphicCells[i];
                        c.x = gc.x.get();
                        c.y = gc.y.get();
                        c.width = gc.width.get();
                        c.height = gc.height.get();

                        if (i < rawMapCells.size()) {
                            const auto& mc = rawMapCells[i];
                            c.cellNumber = mc.cellNumber.get();
                            c.speed = mc.speed.get();
                            c.mapChangeData = mc.mapChangeData.get();
                            c.moveZone = mc.moveZone.get();
                            c.linkedZone = mc.linkedZone.get();
                            c.mov = mc.mov.get();
                            c.los = mc.los.get();
                            c.nonWalkableDuringFight = mc.nonWalkableDuringFight.get();
                            c.nonWalkableDuringRP = mc.nonWalkableDuringRP.get();
                            c.farmCell = mc.farmCell.get();
                            c.visible = mc.visible.get();
                            c.havenbagCell = mc.havenbagCell.get();
                            c.roleplayMonstersMovementBlocked = mc.roleplayMonstersMovementBlocked.get();
                        }
                        states.cells.push_back(c);
                    }
                }
                
                bool cellsDiffer = false;
                if (states.cells.size() != oldCells.size() || oldCells.empty()) {
                    cellsDiffer = true;
                } else {
                    for (size_t i = 0; i < states.cells.size(); ++i) {
                        if (states.cells[i] != oldCells[i]) {
                            cellsDiffer = true;
                            break;
                        }
                    }
                }

                if (cellsDiffer || _cellRefreshRetries > 20) {
                    _lastMapId = states.mapInfo.id;
                    _lastIsInFight = states.isInFight;
                    _cellRefreshRetries = 0;
                    if (fightStateChanged) {
                        ConfigManager::get().state.gridMode = states.isInFight ? "fight" : "rp";
                        ConfigManager::get().save();
                    }
                } else {
                    states.cells = std::move(oldCells);
                    _cellRefreshRetries++;
                }
            } else {
                return;
            }
        } else {
            states.cells = std::move(oldCells);
            states.isInGame = true;
            _cellRefreshRetries = 0;
        }
        
        auto playerSummary = lps.playerSummary.get();
        auto playerHud = lps.playerHuds.get().getValueAt(0);

        states.player.name = playerSummary.characterName.get().c_str();
        states.player.id = playerSummary.characterId.get();
        states.player.breed = playerSummary.classId.get();

        states.player.hp = playerHud.hp.get();
        states.player.hpMax = playerHud.hpMax.get();
        states.player.hpPercent = playerHud.hpPercent.get();
        states.player.ap = playerHud.actionPoints.get();
        states.player.mp = playerHud.movementPoints.get();
        states.player.invocationPoints = playerHud.invocationPoints.get();
        states.player.shield = playerHud.shieldPoints.get();
        
        if (states.isInFight) {
            auto fightService = lps.fightService.get();
            states.currentTurnEntityId = fightService.currentTurnEntityId.get();
            states.isPlayerTurn = (states.currentTurnEntityId == states.player.id);
        }

        auto rawEntitiesMap = lps.entities.get().toUnorderedMap();
        
        std::unordered_map<int64_t, int32_t> teamsMap;
        if (states.isInFight) {
            teamsMap = lps.teamByFighterId.get().toUnorderedMap();
        }

        for (const auto& [id, rawEntity] : rawEntitiesMap) {
            int32_t cellId = rawEntity.cellId.get();
            
            if (id == states.player.id) {
                states.player.cellId = cellId;
                continue; 
            }

            if (cellId <= 0) continue;

            EntityState entityState;
            entityState.id = id;
            entityState.type = rawEntity.type.get();
            entityState.cellId = cellId;
            
            std::string anim = rawEntity.animationState.get().c_str();
            entityState.animationState = anim;
            entityState.isActive = (anim != "AnimStatiqueInactif");

            if (states.isInFight) {
                auto it = teamsMap.find(id);
                if (it != teamsMap.end()) {
                    entityState.teamId = it->second;
                }
                if (entityState.type != EntityType::Portal) {
                    states.fightEntities[id] = std::move(entityState);
                }
            } else {
                if (entityState.type != EntityType::Portal) {
                    states.mapEntities[id] = std::move(entityState);
                }
            }
        }

        auto glyphsMap = lps.glyphs.get();
        for (const auto& [key, glyph] : glyphsMap.toUnorderedMap()) {
            if (glyph.glyphTypeId.get() == static_cast<int32_t>(GlyphType::Portal) && glyph.ownerId.get() == states.player.id) {
                PortalState portal;
                portal.id = glyph.globalIndex.get();
                portal.cellId = glyph.cellId.get();
                portal.isOpen = glyph.active.get();
                states.portalEntities[portal.id] = std::move(portal);
            }
        }

        if (!states.portalEntities.empty()) {
            std::vector<PortalState*> sortedPortals;
            for (auto& [id, portal] : states.portalEntities) {
                sortedPortals.push_back(&portal);
            }
            std::sort(sortedPortals.begin(), sortedPortals.end(),
                      [](const PortalState* a, const PortalState* b) {
                          return a->id < b->id;
                      });
                      
            while (sortedPortals.size() > 4) {
                states.portalEntities.erase(sortedPortals.front()->id);
                sortedPortals.erase(sortedPortals.begin());
            }

            for (size_t i = 0; i < sortedPortals.size(); ++i) {
                sortedPortals[i]->index = static_cast<int32_t>(i + 1);
            }
        }

        if (ConfigManager::get().state.trackedEntityId != 0) {
            int64_t tracked = ConfigManager::get().state.trackedEntityId;
            if (!states.mapEntities.contains(tracked) &&
                !states.fightEntities.contains(tracked) &&
                !states.portalEntities.contains(tracked) &&
                states.player.id != tracked) {
                ConfigManager::get().state.trackedEntityId = 0;
            }
        }
        states.trackedEntityId = ConfigManager::get().state.trackedEntityId;

        auto camNative = er6::FindMainCamera();
        if (camNative) {
            er6::GetCameraMatrix(camNative, states.viewProjMatrix);
        }
    }
};