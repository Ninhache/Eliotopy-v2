#pragma once
#include "InteractiveCellManager.h"
#include "TimeElapsedScopeLogger.h"

class DataStore {
private:
    InteractiveCellManager interactiveCellManager;

public:
    std::vector<ClientCellData> mapCells;
    bool isInGame = false;

    void refresh() {
        TimeElapsedScopeLogger scope("Refresh datastore");
        if (!interactiveCellManager.load()) {
            isInGame = false;
            return;
        }

        auto mapMetadata = interactiveCellManager.mapMetadata.get();
        auto mapData = mapMetadata.mapData.get();
        auto cellList = mapData.cellsList.get();

        if (cellList.isValid()) {
            mapCells = cellList.getAll();
            isInGame = true;
        } else {
            mapCells.clear();
            isInGame = false;
        }
    }
};