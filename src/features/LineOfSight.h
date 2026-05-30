#pragma once
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include "datastore/MapState.h"

class LineOfSight {
public:
    static constexpr int kMapWidth = 14;

    static std::unordered_set<int> visibleFrom(const std::vector<Cell>& cells, int sourceCellId) {
        std::unordered_set<int> visible;
        if (sourceCellId < 0)
            return visible;

        std::unordered_map<int, bool> blocking;
        blocking.reserve(cells.size() * 2);
        for (const auto& cell : cells)
            blocking[cell.cellNumber] = !cell.los;

        visible.reserve(cells.size());
        for (const auto& cell : cells) {
            if (isFree(blocking, sourceCellId, cell.cellNumber))
                visible.insert(cell.cellNumber);
        }
        return visible;
    }

    static int cellParity(int cellId) {
        Coord c = toCoord(cellId);
        return (c.x + c.y) & 1;
    }

    static void cellGridCoord(int cellId, int& x, int& y) {
        Coord c = toCoord(cellId);
        x = c.x;
        y = c.y;
    }

private:
    struct Coord {
        int x;
        int y;
    };

    static Coord toCoord(int cellId) {
        int row = cellId / (kMapWidth * 2);
        int rem = cellId % (kMapWidth * 2);
        if (rem < kMapWidth) {
            int b = rem;
            return { row + b, b - row };
        }
        int b = rem - kMapWidth;
        return { row + 1 + b, b - row };
    }

    static int toCellId(int x, int y) {
        int dx = x - y;
        return dx * kMapWidth + y + dx / 2;
    }

    static bool blocks(const std::unordered_map<int, bool>& blocking, int x, int y) {
        auto it = blocking.find(toCellId(x, y));
        return it != blocking.end() && it->second;
    }

    static void computeNextY(int currentX, int xDirection, int yDirection, double slope, double yAtZero, int& lastY, int& nextY) {
        double yAtCurrentX = (currentX + xDirection * 0.5) * slope + yAtZero;
        if (yDirection > 0) {
            nextY = static_cast<int>(std::floor(yAtCurrentX + 0.5));
            lastY = static_cast<int>(std::ceil(yAtCurrentX - 0.5));
        } else {
            nextY = static_cast<int>(std::ceil(yAtCurrentX - 0.5));
            lastY = static_cast<int>(std::floor(yAtCurrentX + 0.5));
        }
    }

    static bool isFree(const std::unordered_map<int, bool>& blocking, int sourceId, int targetId) {
        if (sourceId == targetId)
            return true;

        Coord source = toCoord(sourceId);
        Coord target = toCoord(targetId);

        if (source.x == target.x) {
            int yDirection = (source.y > target.y) ? -1 : 1;
            int currentY = source.y;
            while (currentY != target.y) {
                currentY += yDirection;
                if (currentY == target.y)
                    break;
                if (blocks(blocking, source.x, currentY))
                    return false;
            }
            return true;
        }

        int xDirection = (source.x > target.x) ? -1 : 1;
        int yDirection = (source.y > target.y) ? -1 : 1;
        double slope = static_cast<double>(target.y - source.y) / static_cast<double>(target.x - source.x);
        double yAtZero = source.y - slope * source.x;

        int currentX = source.x;
        int currentY = source.y;
        int lastY = 0, nextY = 0;
        computeNextY(currentX, xDirection, yDirection, slope, yAtZero, lastY, nextY);

        while (!(currentX == target.x && currentY == target.y)) {
            currentY += yDirection;
            if (currentY * yDirection > lastY * yDirection) {
                currentX += xDirection;
                currentY = nextY;
                computeNextY(currentX, xDirection, yDirection, slope, yAtZero, lastY, nextY);
            }
            if (currentX == target.x && currentY == target.y)
                break;
            if (blocks(blocking, currentX, currentY))
                return false;
        }
        return true;
    }
};
