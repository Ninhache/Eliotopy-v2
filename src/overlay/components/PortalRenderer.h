#pragma once
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <wrl/client.h>
#include "RenderContext.h"
#include "Config.h"
#include "features/RedirectionHelper.h"
#include "features/PortalPlanner.h"
#include "features/LineOfSight.h"

class PortalRenderer : public IOverlayComponent {
public:
    bool update(const RenderContext& ctx, const GameState& state) override {
        PortalPlanner::get().setHoveredCell(ctx.hoveredCell);

        std::vector<std::pair<int, bool>> memory = sortedMemory(state);
        PortalPlanner::get().setMemory(memory);

        std::vector<int> netCells;
        std::vector<PortalPlanner::Manual> manual = PortalPlanner::get().manualPortals();
        if (!manual.empty())
            for (const auto& m : manual) netCells.push_back(m.cellId);
        else
            for (const auto& [cellId, open] : memory) netCells.push_back(cellId);
        PortalPlanner::get().setNetworkCells(netCells);

        int target = g_portalEntranceTarget.load();
        if (target >= 0 && std::find(netCells.begin(), netCells.end(), target) == netCells.end())
            g_portalEntranceTarget = -1;

        bool preview = g_portalPreviewActive.load() || (g_portalEntranceTarget.load() >= 0) || g_distanceMeasureActive.load();
        std::string signature = networkSignature(state);

        bool dirty = preview || (preview != _lastPreview) || (signature != _lastSignature);
        _lastPreview = preview;
        _lastSignature = signature;
        return dirty;
    }

    void render(const RenderContext& ctx, const GameState& state) override {
        if (!ctx.valid || !ctx.cellById)
            return;

        const auto& config = ConfigManager::get().state;
        std::vector<Portal> network = buildNetwork(state, *ctx.cellById);
        std::vector<Portal> display = network;
        std::vector<Portal> path;

        int entranceTarget = g_portalEntranceTarget.load();
        bool hoveredWalkable = isWalkable(state, *ctx.cellById, ctx.hoveredCell);
        bool hoveredIsPortal = false;
        for (const auto& portal : network)
            if (portal.cellId == ctx.hoveredCell) { hoveredIsPortal = true; break; }

        if (g_portalPreviewActive.load() && hoveredWalkable) {
            path = buildPreview(ctx.hoveredCell, network, display, config);
        } else if (entranceTarget >= 0 && hoveredWalkable && !hoveredIsPortal) {
            std::vector<Portal> sim = network;
            std::vector<Portal> simPath = buildPreview(ctx.hoveredCell, network, sim, config);
            if (!simPath.empty() && simPath.back().cellId == entranceTarget) {
                display = sim;
                path = simPath;
            }
        }

        if (config.portalGreyDeleted)
            appendGhosts(ctx, display);

        if (entranceTarget >= 0)
            drawEntranceCandidates(ctx, state, network, entranceTarget, config);

        for (const auto& portal : display)
            drawPortalShape(ctx, portal, config);

        if (!path.empty())
            drawPath(ctx, path, config);

        if (config.portalShowNumber)
            for (const auto& portal : display)
                drawNumber(ctx, portal, config);

        if (!path.empty() && config.damageShow)
            drawDamage(ctx, path, config);

        drawDistanceMeasure(ctx);
    }

    void drawDistanceMeasure(const RenderContext& ctx) {
        if (!g_distanceMeasureActive.load())
            return;
        int anchor = g_distanceAnchorCell.load();
        int current = ctx.hoveredCell;
        if (anchor < 0 || current < 0)
            return;
        auto ita = ctx.cellById->find(anchor);
        auto itb = ctx.cellById->find(current);
        if (ita == ctx.cellById->end() || itb == ctx.cellById->end())
            return;

        int ax, ay, bx, by;
        LineOfSight::cellGridCoord(anchor, ax, ay);
        LineOfSight::cellGridCoord(current, bx, by);
        int dist = std::abs(ax - bx) + std::abs(ay - by);

        float acx = ctx.cellX(*ita->second);
        float acy = ctx.cellY(*ita->second);
        float bcx = ctx.cellX(*itb->second);
        float bcy = ctx.cellY(*itb->second);

        ctx.drawLine(acx, acy, bcx, bcy, D2D1::ColorF(0.62f, 0.62f, 0.68f, 0.9f), 3.0f);
        ctx.fillRect(bcx - 12.0f, bcy - 9.0f, bcx + 12.0f, bcy + 9.0f, D2D1::ColorF(0, 0, 0, 0.6f));
        ctx.drawTextCentered(std::to_wstring(dist), bcx, bcy, D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
    }

    bool isWalkable(const GameState& state, const std::unordered_map<int, const Cell*>& cells, int cellId) const {
        if (cellId < 0)
            return false;
        auto it = cells.find(cellId);
        if (it == cells.end())
            return false;
        const Cell& c = *it->second;
        return state.isInFight ? (c.mov && !c.nonWalkableDuringFight) : (c.mov && !c.nonWalkableDuringRP);
    }

private:
    bool _lastPreview = false;
    std::string _lastSignature;

    static const std::string& portalColor(const ConfigState& config, int number) {
        switch (number) {
            case 1:  return config.portalColor1;
            case 2:  return config.portalColor2;
            case 3:  return config.portalColor3;
            default: return config.portalColor4;
        }
    }

    std::string networkSignature(const GameState& state) const {
        const auto& c = ConfigManager::get().state;
        std::string s = c.portalColor1 + c.portalColor2 + c.portalColor3 + c.portalColor4 + c.connectorColor +
                        c.closedPortalColor + (c.closedPortalFilled ? "F" : "O") +
                        c.portalNumberColor + c.portalNumberShape +
                        std::to_string(c.portalOpacity) + (c.portalFilled ? "f" : "o") + (c.portalShowNumber ? "n" : "") +
                        (c.portalShowDistance ? "d" : "") + std::to_string(c.portalThickness) +
                        std::to_string(c.connectorOpacity) + std::to_string(c.connectorThickness) +
                        std::to_string(c.portalNumberOffsetX) + std::to_string(c.portalNumberOffsetY) +
                        std::to_string(c.portalNumberScale) + (c.portalGreyDeleted ? "g" : "") +
                        c.entranceColor + std::to_string(c.entranceOpacity) + (c.entranceFilled ? "f" : "o") +
                        std::to_string(c.entranceThickness) + (c.damageShow ? "D" : "") + c.damageColor +
                        std::to_string(c.damageThickness) + std::to_string(c.damageScale) +
                        (c.damageOutline ? "O" : "") + c.damageOutlineColor + std::to_string(c.damageOutlineThickness) + "|";
        for (const auto& [id, portal] : state.portalEntities)
            s += std::to_string(portal.cellId) + (portal.isOpen ? "o" : "c") + ";";
        for (const auto& manual : PortalPlanner::get().manualPortals())
            s += "m" + std::to_string(manual.cellId) + (manual.open ? "o" : "c") + ";";
        for (int cellId : PortalPlanner::get().removedPortals())
            s += "r" + std::to_string(cellId) + ";";
        return s;
    }

    std::vector<std::pair<int, bool>> sortedMemory(const GameState& state) const {
        std::vector<const PortalState*> memory;
        for (const auto& [id, portal] : state.portalEntities)
            memory.push_back(&portal);
        std::sort(memory.begin(), memory.end(), [](const PortalState* a, const PortalState* b) {
            return a->index < b->index;
        });
        std::vector<std::pair<int, bool>> out;
        out.reserve(memory.size());
        for (const PortalState* portal : memory)
            out.push_back({ portal->cellId, portal->isOpen });
        return out;
    }

    std::vector<Portal> buildNetwork(const GameState& state, const std::unordered_map<int, const Cell*>& cells) const {
        std::vector<Portal> network;
        std::unordered_set<int> seen;

        auto add = [&](int cellId, bool open) {
            if (cellId < 0 || network.size() >= 4 || seen.count(cellId) > 0 || cells.find(cellId) == cells.end())
                return;
            Portal portal;
            portal.cellId = cellId;
            portal.open = open;
            LineOfSight::cellGridCoord(cellId, portal.x, portal.y);
            network.push_back(portal);
            seen.insert(cellId);
        };

        std::vector<PortalPlanner::Manual> manual = PortalPlanner::get().manualPortals();
        if (!manual.empty()) {
            for (const auto& m : manual)
                add(m.cellId, m.open);
        } else {
            for (const auto& [cellId, open] : sortedMemory(state))
                add(cellId, open);
        }

        for (size_t i = 0; i < network.size(); ++i)
            network[i].number = static_cast<int>(i) + 1;
        return network;
    }

    std::vector<Portal> buildPreview(int hovered, const std::vector<Portal>& network, std::vector<Portal>& display, const ConfigState& config) {
        for (const auto& portal : network) {
            if (portal.cellId == hovered) {
                display = network;
                return RedirectionHelper::calculatePath(network, portal);
            }
        }

        display = network;

        Portal dropped;
        bool hasDropped = false;
        if (display.size() >= 4) {
            for (auto it = display.begin(); it != display.end();) {
                if (it->number == 1) {
                    dropped = *it;
                    hasDropped = true;
                    it = display.erase(it);
                } else {
                    it->number -= 1;
                    ++it;
                }
            }
        }

        Portal entrance;
        entrance.cellId = hovered;
        entrance.open = true;
        entrance.number = static_cast<int>(display.size()) + 1;
        LineOfSight::cellGridCoord(hovered, entrance.x, entrance.y);
        display.push_back(entrance);

        std::vector<Portal> path = RedirectionHelper::calculatePath(display, entrance);

        if (hasDropped && config.portalGreyDeleted) {
            dropped.ghost = true;
            display.push_back(dropped);
        }

        return path;
    }

    void drawPortalShape(const RenderContext& ctx, const Portal& portal, const ConfigState& config) {
        const Cell* cell = ctx.cellById->at(portal.cellId);
        float cx = ctx.cellX(*cell);
        float cy = ctx.cellY(*cell);
        float hw = ctx.cellHalfW(*cell);
        float hh = ctx.cellHalfH(*cell);

        float alpha = config.portalOpacity / 100.0f;
        bool grey = portal.ghost || !portal.open;
        D2D1_COLOR_F color = grey ? RenderContext::parseHexColor(config.closedPortalColor, alpha)
                                  : RenderContext::parseHexColor(portalColor(config, portal.number), alpha);
        bool filled = grey ? config.closedPortalFilled : config.portalFilled;

        if (filled)
            ctx.fillDiamond(cx, cy, hw, hh, color);
        else
            ctx.drawDiamondOutline(cx, cy, hw, hh, color, static_cast<float>(config.portalThickness));
    }

    void drawNumber(const RenderContext& ctx, const Portal& portal, const ConfigState& config) {
        const Cell* cell = ctx.cellById->at(portal.cellId);
        float cx = ctx.cellX(*cell);
        float cy = ctx.cellY(*cell);
        float hw = ctx.cellHalfW(*cell);
        float hh = ctx.cellHalfH(*cell);

        bool grey = portal.ghost || !portal.open;
        D2D1_COLOR_F badge = grey ? RenderContext::parseHexColor(config.closedPortalColor, 1.0f)
                                  : RenderContext::parseHexColor(portalColor(config, portal.number), 1.0f);
        D2D1_COLOR_F number = RenderContext::parseHexColor(config.portalNumberColor, 1.0f);

        float scale = config.portalNumberScale / 100.0f;
        float radius = hh * 0.82f * scale;
        float bx = cx - hw * 0.42f + static_cast<float>(config.portalNumberOffsetX);
        float by = cy - hh * 0.62f + static_cast<float>(config.portalNumberOffsetY);

        if (config.portalNumberShape == "rect")
            ctx.fillRoundedRect(bx, by, radius, radius, radius * 0.32f, badge);
        else if (config.portalNumberShape == "diamond")
            ctx.fillDiamond(bx, by, radius * 1.18f, radius * 1.18f, badge);
        else
            ctx.fillCircle(bx, by, radius, badge);

        if (!portal.ghost)
            ctx.drawTextScaled(std::to_wstring(portal.number), bx, by, radius * 1.2f, number);
    }

    void appendGhosts(const RenderContext& ctx, std::vector<Portal>& display) const {
        for (int cellId : PortalPlanner::get().removedPortals()) {
            if (ctx.cellById->find(cellId) == ctx.cellById->end())
                continue;
            bool already = false;
            for (const auto& p : display)
                if (p.cellId == cellId) { already = true; break; }
            if (already)
                continue;
            Portal ghost;
            ghost.cellId = cellId;
            ghost.ghost = true;
            LineOfSight::cellGridCoord(cellId, ghost.x, ghost.y);
            display.push_back(ghost);
        }
    }

    void drawEntranceCandidates(const RenderContext& ctx, const GameState& state, const std::vector<Portal>& network, int target, const ConfigState& config) {
        bool valid = false;
        for (const auto& portal : network)
            if (portal.cellId == target) { valid = true; break; }
        if (!valid)
            return;

        D2D1_COLOR_F color = RenderContext::parseHexColor(config.entranceColor, config.entranceOpacity / 100.0f);

        for (const auto& [cellId, cell] : *ctx.cellById) {
            if (!isWalkable(state, *ctx.cellById, cellId))
                continue;

            bool isPortal = false;
            for (const auto& portal : network)
                if (portal.cellId == cellId) { isPortal = true; break; }
            if (isPortal)
                continue;

            std::vector<Portal> sim = network;
            bool targetDropped = false;
            if (sim.size() >= 4) {
                for (auto it = sim.begin(); it != sim.end();) {
                    if (it->number == 1) {
                        if (it->cellId == target) targetDropped = true;
                        it = sim.erase(it);
                    } else {
                        it->number -= 1;
                        ++it;
                    }
                }
            }
            if (targetDropped)
                continue;

            Portal entrance;
            entrance.cellId = cellId;
            entrance.open = true;
            entrance.number = static_cast<int>(sim.size()) + 1;
            LineOfSight::cellGridCoord(cellId, entrance.x, entrance.y);
            sim.push_back(entrance);

            std::vector<Portal> path = RedirectionHelper::calculatePath(sim, entrance);
            if (path.empty() || path.back().cellId != target)
                continue;

            float cx = ctx.cellX(*cell);
            float cy = ctx.cellY(*cell);
            float hw = ctx.cellHalfW(*cell);
            float hh = ctx.cellHalfH(*cell);
            if (config.entranceFilled)
                ctx.fillDiamond(cx, cy, hw, hh, color);
            else
                ctx.drawDiamondOutline(cx, cy, hw, hh, color, static_cast<float>(config.entranceThickness));
        }
    }

    void drawDamage(const RenderContext& ctx, const std::vector<Portal>& path, const ConfigState& config) {
        int totalDist = 0;
        for (size_t i = 0; i + 1 < path.size(); ++i)
            totalDist += std::abs(path[i].x - path[i + 1].x) + std::abs(path[i].y - path[i + 1].y);
        if (totalDist <= 0)
            return;

        const Cell* entrance = ctx.cellById->at(path.front().cellId);
        float cx = ctx.cellX(*entrance);
        float cy = ctx.cellY(*entrance);
        float hw = ctx.cellHalfW(*entrance);
        float hh = ctx.cellHalfH(*entrance);

        float scale = config.damageScale / 100.0f;
        float fontSize = hh * 1.1f * scale;
        int level = std::max(1, std::min(9, config.damageThickness));
        DWRITE_FONT_WEIGHT weight = static_cast<DWRITE_FONT_WEIGHT>(level * 100);
        D2D1_COLOR_F color = RenderContext::parseHexColor(config.damageColor, 1.0f);
        std::wstring text = L"+" + std::to_wstring(totalDist * 2) + L"%";
        float tx = cx + hw * 0.7f;
        float ty = cy - hh * 1.3f;

        if (config.damageOutline) {
            D2D1_COLOR_F outline = RenderContext::parseHexColor(config.damageOutlineColor, 1.0f);
            float d = static_cast<float>(config.damageOutlineThickness);
            const float dirs[8][2] = { {-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1} };
            for (const auto& dir : dirs)
                ctx.drawTextScaled(text, tx + dir[0] * d, ty + dir[1] * d, fontSize, outline, weight);
        }

        ctx.drawTextScaled(text, tx, ty, fontSize, color, weight);
    }

    static D2D1_POINT_2F lerp(const D2D1_POINT_2F& a, const D2D1_POINT_2F& b, float t) {
        return D2D1::Point2F(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
    }

    void drawPath(const RenderContext& ctx, const std::vector<Portal>& path, const ConfigState& config) {
        if (path.size() < 2)
            return;

        D2D1_COLOR_F flow = RenderContext::parseHexColor(config.connectorColor, config.connectorOpacity / 100.0f);
        float width = static_cast<float>(config.connectorThickness);

        std::vector<D2D1_POINT_2F> points;
        points.reserve(path.size());
        for (const auto& portal : path) {
            const Cell* cell = ctx.cellById->at(portal.cellId);
            points.push_back(D2D1::Point2F(ctx.cellX(*cell), ctx.cellY(*cell)));
        }

        ctx.brush->SetColor(flow);
        for (size_t i = 0; i + 1 < points.size(); ++i)
            ctx.target->DrawLine(points[i], points[i + 1], ctx.brush, width);

        drawFlow(ctx, points, flow, width);

        if (config.portalShowDistance) {
            for (size_t i = 0; i + 1 < path.size(); ++i) {
                int distance = std::abs(path[i].x - path[i + 1].x) + std::abs(path[i].y - path[i + 1].y);
                float mx = (points[i].x + points[i + 1].x) * 0.5f;
                float my = (points[i].y + points[i + 1].y) * 0.5f;
                ctx.fillRect(mx - 12.0f, my - 9.0f, mx + 12.0f, my + 9.0f, D2D1::ColorF(0, 0, 0, 0.6f));
                ctx.drawTextCentered(std::to_wstring(distance), mx, my, D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
            }
        }

        const Portal& exitPortal = path.back();
        const Cell* exit = ctx.cellById->at(exitPortal.cellId);
        float alpha = 0.8f * static_cast<float>((std::sin(GetTickCount64() / 160.0) + 1.0) / 2.0);
        D2D1_COLOR_F exitColor = RenderContext::parseHexColor(portalColor(config, exitPortal.number), alpha);
        ctx.fillDiamond(ctx.cellX(*exit), ctx.cellY(*exit), ctx.cellHalfW(*exit), ctx.cellHalfH(*exit), exitColor);
    }

    void drawFlow(const RenderContext& ctx, const std::vector<D2D1_POINT_2F>& points, const D2D1_COLOR_F& base, float width) {
        std::vector<float> lengths;
        float total = 0.0f;
        for (size_t i = 0; i + 1 < points.size(); ++i) {
            float dx = points[i + 1].x - points[i].x;
            float dy = points[i + 1].y - points[i].y;
            float len = std::sqrt(dx * dx + dy * dy);
            lengths.push_back(len);
            total += len;
        }
        if (total < 1.0f)
            return;

        float cometLen = 26.0f;
        float t = static_cast<float>((GetTickCount64() % 1400) / 1400.0);
        float head = t * (total + cometLen);
        float tail = head - cometLen;

        D2D1_COLOR_F bright = D2D1::ColorF(base.r + (1.0f - base.r) * 0.6f,
                                          base.g + (1.0f - base.g) * 0.6f,
                                          base.b + (1.0f - base.b) * 0.6f, base.a);
        ctx.brush->SetColor(bright);

        float acc = 0.0f;
        for (size_t i = 0; i + 1 < points.size(); ++i) {
            float d0 = acc;
            float d1 = acc + lengths[i];
            float lo = std::max(tail, d0);
            float hi = std::min(head, d1);
            if (hi > lo && lengths[i] > 0.01f) {
                float f0 = (lo - d0) / lengths[i];
                float f1 = (hi - d0) / lengths[i];
                ctx.target->DrawLine(lerp(points[i], points[i + 1], f0),
                                     lerp(points[i], points[i + 1], f1),
                                     ctx.brush, width + 1.0f);
            }
            acc = d1;
        }
    }
};
