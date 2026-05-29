#pragma once
#include <unordered_set>
#include "RenderContext.h"
#include "Config.h"
#include "features/LineOfSight.h"

class LosRenderer : public IOverlayComponent {
public:
    bool update(const RenderContext& ctx, const GameState& state) override {
        bool active = g_losAssistActive.load();
        int hovered = -1;

        if (active && ctx.valid && !state.cells.empty()) {
            const Cell* cell = findHovered(ctx, state);
            if (cell) {
                bool walkable = state.isInFight ? (cell->mov && !cell->nonWalkableDuringFight)
                                                : (cell->mov && !cell->nonWalkableDuringRP);
                if (walkable)
                    hovered = cell->cellNumber;
            }
        }

        bool dirty = (active != _active) || (active && hovered != _hovered);

        if (active && (hovered != _hovered || state.cells.size() != _lastCellCount))
            _visible = LineOfSight::visibleFrom(state.cells, hovered);

        _hovered = hovered;
        _active = active;
        _lastCellCount = state.cells.size();
        return dirty;
    }

    void render(const RenderContext& ctx, const GameState& state) override {
        if (!_active || _hovered == -1 || !ctx.valid || !state.isInGame || state.cells.empty())
            return;

        const auto& config = ConfigManager::get().state;
        D2D1_COLOR_F shadeEven = RenderContext::parseHexColor(config.losEvenColor, config.losEvenOpacity / 100.0f);
        D2D1_COLOR_F shadeOdd = RenderContext::parseHexColor(config.losOddColor, config.losOddOpacity / 100.0f);
        D2D1_COLOR_F shadeWall = RenderContext::parseHexColor(config.losWallColor, config.losWallOpacity / 100.0f);
        bool showWalls = config.losShowWalls;
        bool inFight = state.isInFight;

        for (const auto& cell : state.cells) {
            const D2D1_COLOR_F* shade = nullptr;

            if (!cell.los) {
                if (!showWalls)
                    continue;
                shade = &shadeWall;
            } else {
                if (_visible.count(cell.cellNumber) > 0)
                    continue;
                bool walkable = inFight ? (cell.mov && !cell.nonWalkableDuringFight)
                                        : (cell.mov && !cell.nonWalkableDuringRP);
                if (!walkable)
                    continue;
                shade = LineOfSight::cellParity(cell.cellNumber) ? &shadeOdd : &shadeEven;
            }

            ctx.fillDiamond(ctx.cellX(cell), ctx.cellY(cell), ctx.cellHalfW(cell), ctx.cellHalfH(cell), *shade);
        }
    }

private:
    const Cell* findHovered(const RenderContext& ctx, const GameState& state) const {
        const Cell* best = nullptr;
        double bestDist = 1e18;
        for (const auto& cell : state.cells) {
            double dx = ctx.cursor.x - ctx.cellX(cell);
            double dy = ctx.cursor.y - ctx.cellY(cell);
            double dist = dx * dx + dy * dy;
            if (dist < bestDist) {
                bestDist = dist;
                best = &cell;
            }
        }
        return best;
    }

    std::unordered_set<int> _visible;
    int _hovered = -1;
    bool _active = false;
    size_t _lastCellCount = 0;
};
