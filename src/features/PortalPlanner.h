#pragma once
#include <windows.h>
#include <vector>
#include <utility>
#include <atomic>
#include <mutex>
#include <algorithm>

class PortalPlanner {
public:
    struct Manual {
        int number = 0;
        int cellId = -1;
        bool open = true;
    };

    static PortalPlanner& get() {
        static PortalPlanner instance;
        return instance;
    }

    void setHoveredCell(int cellId) { _hovered.store(cellId); }
    int hoveredCell() const { return _hovered.load(); }

    void setMemory(std::vector<std::pair<int, bool>> memory) {
        std::lock_guard<std::mutex> lock(_mutex);
        _memory = std::move(memory);
    }

    void setNetworkCells(std::vector<int> cells) {
        std::lock_guard<std::mutex> lock(_mutex);
        _networkCells = std::move(cells);
    }

    bool isNetworkCell(int cellId) const {
        std::lock_guard<std::mutex> lock(_mutex);
        return std::find(_networkCells.begin(), _networkCells.end(), cellId) != _networkCells.end();
    }

    void place() {
        int hovered = _hovered.load();
        if (hovered < 0)
            return;

        std::lock_guard<std::mutex> lock(_mutex);

        if (_queue.empty())
            for (const auto& memory : _memory)
                _queue.push_back({ memory.first, memory.second });

        ULONGLONG now = GetTickCount64();
        bool doublePress = (_lastCell == hovered) && (now - _lastTick <= 350);
        _lastTick = now;
        _lastCell = hovered;

        for (auto it = _queue.begin(); it != _queue.end(); ++it) {
            if (it->cellId == hovered) {
                if (doublePress)
                    _queue.erase(it);
                else
                    it->open = !it->open;
                return;
            }
        }

        _removed.erase(std::remove(_removed.begin(), _removed.end(), hovered), _removed.end());
        _queue.push_back({ hovered, true });
        if (_queue.size() > 4) {
            int dropped = _queue.front().cellId;
            _queue.erase(_queue.begin());
            _removed.erase(std::remove(_removed.begin(), _removed.end(), dropped), _removed.end());
            _removed.push_back(dropped);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.clear();
        _removed.clear();
    }

    std::vector<int> removedPortals() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _removed;
    }

    std::vector<Manual> manualPortals() const {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<Manual> out;
        out.reserve(_queue.size());
        for (size_t i = 0; i < _queue.size(); ++i)
            out.push_back({ static_cast<int>(i) + 1, _queue[i].cellId, _queue[i].open });
        return out;
    }

private:
    struct Entry {
        int cellId = -1;
        bool open = true;
    };

    std::vector<Entry> _queue;
    std::vector<std::pair<int, bool>> _memory;
    std::vector<int> _removed;
    std::vector<int> _networkCells;
    std::atomic<int> _hovered{ -1 };
    ULONGLONG _lastTick = 0;
    int _lastCell = -1;
    mutable std::mutex _mutex;
};
