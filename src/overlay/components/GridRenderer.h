#pragma once
#include <cstdint>
#include <string>
#include "RenderContext.h"
#include "Config.h"

class GridRenderer : public IOverlayComponent {
public:
    bool update(const RenderContext&, const GameState& state) override {
        const auto& config = ConfigManager::get().state;
        _trackedCellId = resolveTrackedCellId(state);
        _blinkOn = ((GetTickCount64() / 250) % 2) == 0;

        bool dirty =
            config.gridOverlay != _lastGridOverlay ||
            config.gridMode != _lastGridMode ||
            config.gridShowLos != _lastShowLos ||
            config.gridShowMove != _lastShowMove ||
            config.gridColor != _lastGridColor ||
            config.gridOpacity != _lastGridOpacity ||
            config.gridFillColor != _lastFillColor ||
            config.gridFillOpacity != _lastFillOpacity ||
            _trackedCellId != _lastTrackedCellId ||
            (_trackedCellId != -1 && _blinkOn != _lastBlinkOn);

        _lastGridOverlay = config.gridOverlay;
        _lastGridMode = config.gridMode;
        _lastShowLos = config.gridShowLos;
        _lastShowMove = config.gridShowMove;
        _lastGridColor = config.gridColor;
        _lastGridOpacity = config.gridOpacity;
        _lastFillColor = config.gridFillColor;
        _lastFillOpacity = config.gridFillOpacity;
        _lastTrackedCellId = _trackedCellId;
        _lastBlinkOn = _blinkOn;
        return dirty;
    }

    void render(const RenderContext& ctx, const GameState& state) override {
        if (!ctx.valid || !state.isInGame || state.cells.empty())
            return;

        const auto& config = ConfigManager::get().state;
        bool gridOverlay = config.gridOverlay;
        if (!gridOverlay && _trackedCellId == -1)
            return;

        D2D1_COLOR_F gridColor = RenderContext::parseHexColor(config.gridColor, config.gridOpacity / 100.0f);
        D2D1_COLOR_F fillColor = RenderContext::parseHexColor(config.gridFillColor, config.gridFillOpacity / 100.0f);
        D2D1_COLOR_F trackColor = D2D1::ColorF(180.0f / 255.0f, 0.0f, 0.0f, 1.0f);
        const std::string& gridMode = config.gridMode;
        bool showLos = config.gridShowLos;
        bool showMove = config.gridShowMove;

        for (const auto& cell : state.cells) {
            bool shouldDraw = false;
            bool isLosBlocked = false;

            if (gridMode == "rp")
                shouldDraw = cell.mov && !cell.nonWalkableDuringRP;
            else if (gridMode == "fight")
                shouldDraw = cell.mov && !cell.nonWalkableDuringFight;

            if (showMove && cell.mov)
                shouldDraw = true;

            if (showLos && !cell.los) {
                shouldDraw = true;
                isLosBlocked = true;
            }

            if (!gridOverlay) {
                shouldDraw = false;
                isLosBlocked = false;
            }

            bool isTracked = (_trackedCellId != -1 && cell.cellNumber == _trackedCellId);
            if (!shouldDraw && !isTracked)
                continue;

            float cx = ctx.cellX(cell);
            float cy = ctx.cellY(cell);
            float hw = ctx.cellHalfW(cell);
            float hh = ctx.cellHalfH(cell);

            if (shouldDraw) {
                if (isLosBlocked)
                    ctx.fillDiamond(cx, cy, hw, hh, fillColor);
                else
                    ctx.drawDiamondOutline(cx, cy, hw, hh, gridColor, 1.0f);
            }

            if (isTracked) {
                if (_blinkOn)
                    ctx.fillDiamond(cx, cy, hw, hh, trackColor);
                ctx.drawDiamondOutline(cx, cy, hw, hh, trackColor, 2.0f);
            }
        }
    }

private:
    static int32_t resolveTrackedCellId(const GameState& state) {
        if (state.trackedEntityId == 0)
            return -1;
        if (state.trackedEntityId == state.player.id)
            return state.player.cellId;
        if (auto it = state.mapEntities.find(state.trackedEntityId); it != state.mapEntities.end())
            return it->second.cellId;
        if (auto it = state.fightEntities.find(state.trackedEntityId); it != state.fightEntities.end())
            return it->second.cellId;
        if (auto it = state.portalEntities.find(state.trackedEntityId); it != state.portalEntities.end())
            return it->second.cellId;
        return -1;
    }

    int32_t _trackedCellId = -1;
    bool _blinkOn = false;

    bool _lastGridOverlay = true;
    std::string _lastGridMode = "rp";
    bool _lastShowLos = false;
    bool _lastShowMove = false;
    std::string _lastGridColor;
    int _lastGridOpacity = 100;
    std::string _lastFillColor;
    int _lastFillOpacity = 60;
    int32_t _lastTrackedCellId = -1;
    bool _lastBlinkOn = false;
};
