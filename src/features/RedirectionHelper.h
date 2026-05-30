#pragma once
#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>

struct Portal {
    int number = 0;
    int cellId = -1;
    int x = 0;
    int y = 0;
    bool open = true;
    bool ghost = false;
};

class RedirectionHelper {
public:
    static std::vector<Portal> calculatePath(const std::vector<Portal>& network, Portal entrance) {
        std::vector<Portal> path;
        if (network.empty())
            return path;

        std::vector<Portal> open;
        open.reserve(network.size());
        for (const auto& p : network)
            if (p.open)
                open.push_back(p);
        if (open.empty())
            return path;

        bool found = false;
        for (const auto& p : open) {
            if (sameCell(p, entrance)) {
                entrance = p;
                found = true;
                break;
            }
        }
        if (!found)
            return path;

        std::vector<Portal> unvisited;
        for (const auto& p : open)
            if (!sameCell(p, entrance))
                unvisited.push_back(p);

        Portal current = entrance;
        int maxTry = static_cast<int>(unvisited.size()) + 1;
        while (maxTry-- > 0) {
            path.push_back(current);
            if (unvisited.empty())
                break;
            int next = getClosestPortal(current, unvisited);
            if (next < 0)
                break;
            current = unvisited[next];
            unvisited.erase(unvisited.begin() + next);
        }

        if (path.size() < 2)
            return std::vector<Portal>{ entrance };
        return path;
    }

private:
    enum AngleRelation { SAME, OPPOSITE, TRIGONOMETRIC, COUNTER_TRIGONOMETRIC };

    struct Coord { int x = 0; int y = 0; };
    struct Candidate { int index = -1; Coord coord; double angle = 0.0; };

    static constexpr double PI = 3.14159265358979323846;
    static constexpr double TWO_PI = 2.0 * PI;
    static constexpr double EPS = 1e-9;

    static bool sameCell(const Portal& a, const Portal& b) { return a.x == b.x && a.y == b.y; }
    static Coord toCoord(const Portal& p) { return { p.x, p.y }; }
    static int portalDistance(const Portal& a, const Portal& b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }
    static Coord vectorFromTo(const Coord& s, const Coord& e) { return { e.x - s.x, e.y - s.y }; }
    static int determinant(const Coord& a, const Coord& b) { return a.x * b.y - a.y * b.x; }

    static double euclideanDistance(const Coord& a, const Coord& b) {
        double dx = static_cast<double>(a.x) - b.x;
        double dy = static_cast<double>(a.y) - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    static double clampDouble(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

    static AngleRelation compareAngles(const Coord& ref, const Coord& start, const Coord& end) {
        Coord a = vectorFromTo(ref, start);
        Coord b = vectorFromTo(ref, end);
        int det = determinant(a, b);
        if (det != 0)
            return det > 0 ? TRIGONOMETRIC : COUNTER_TRIGONOMETRIC;
        bool sameX = (a.x >= 0) == (b.x >= 0);
        bool sameY = (a.y >= 0) == (b.y >= 0);
        return (sameX && sameY) ? SAME : OPPOSITE;
    }

    static double getAngle(const Coord& ref, const Coord& a, const Coord& b) {
        double sideA = euclideanDistance(a, b);
        double sideB = euclideanDistance(ref, a);
        double sideC = euclideanDistance(ref, b);
        double denom = 2.0 * sideB * sideC;
        if (denom <= EPS)
            return 0.0;
        double cosValue = ((sideB * sideB) + (sideC * sideC) - (sideA * sideA)) / denom;
        return std::acos(clampDouble(cosValue, -1.0, 1.0));
    }

    static double getPositiveOrientedAngle(const Coord& ref, const Coord& start, const Coord& end) {
        switch (compareAngles(ref, start, end)) {
            case SAME:                  return 0.0;
            case OPPOSITE:              return PI;
            case TRIGONOMETRIC:         return getAngle(ref, start, end);
            case COUNTER_TRIGONOMETRIC: return TWO_PI - getAngle(ref, start, end);
            default:                    return 0.0;
        }
    }

    static int getBestPortalWhenRefIsNotInsideClosests(const Coord& ref, const std::vector<Candidate>& sorted) {
        if (sorted.size() < 2)
            return -1;
        int prev = static_cast<int>(sorted.size()) - 1;
        for (int i = 0; i < static_cast<int>(sorted.size()); ++i) {
            AngleRelation relation = compareAngles(ref, sorted[prev].coord, sorted[i].coord);
            if (relation == OPPOSITE) {
                if (sorted.size() <= 2)
                    return -1;
                return sorted[prev].index;
            }
            if (relation == COUNTER_TRIGONOMETRIC)
                return sorted[prev].index;
            prev = i;
        }
        return -1;
    }

    static int getBestNextPortal(const Portal& ref, const std::vector<Portal>& unvisited, const std::vector<int>& closest) {
        if (closest.size() < 2)
            return closest.empty() ? -1 : closest[0];

        Coord refCoord = toCoord(ref);
        Coord nudge = { refCoord.x, refCoord.y + 1 };

        std::vector<Candidate> sorted;
        sorted.reserve(closest.size());
        for (int idx : closest) {
            Coord c = toCoord(unvisited[idx]);
            sorted.push_back({ idx, c, getPositiveOrientedAngle(refCoord, nudge, c) });
        }

        std::sort(sorted.begin(), sorted.end(), [](const Candidate& a, const Candidate& b) {
            if (std::abs(a.angle - b.angle) > EPS)
                return a.angle < b.angle;
            return a.index < b.index;
        });

        int special = getBestPortalWhenRefIsNotInsideClosests(refCoord, sorted);
        if (special != -1)
            return special;
        return sorted[0].index;
    }

    static int getClosestPortal(const Portal& ref, const std::vector<Portal>& unvisited) {
        int best = std::numeric_limits<int>::max();
        std::vector<int> closest;
        for (int i = 0; i < static_cast<int>(unvisited.size()); ++i) {
            int d = portalDistance(ref, unvisited[i]);
            if (d < best) {
                best = d;
                closest.clear();
                closest.push_back(i);
            } else if (d == best) {
                closest.push_back(i);
            }
        }
        if (closest.empty())
            return -1;
        if (closest.size() == 1)
            return closest[0];
        return getBestNextPortal(ref, unvisited, closest);
    }
};
