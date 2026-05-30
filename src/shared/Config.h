#pragma once
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#include <string>
#include <fstream>
#include <unordered_map>
#include "json.hpp"
#include <atomic>
#include "AppLogger.h"

inline std::atomic<int> g_memoryPollRate{250};
inline std::atomic<bool> g_losAssistActive{false};
inline std::atomic<bool> g_portalPreviewActive{false};
inline std::atomic<int> g_portalEntranceTarget{-1};
inline std::atomic<bool> g_distanceMeasureActive{false};
inline std::atomic<int> g_distanceAnchorCell{-1};

struct ConfigState {
    int memoryPollRate = 250;
    bool gridOverlay = false;
    std::string gridColor = "#ffffff";
    int gridOpacity = 100;
    std::string gridMode = "rp";
    bool gridShowLos = true;
    bool gridShowMove = false;
    int gridFillOpacity = 60;
    std::string gridFillColor = "#555555";
    int backgroundOverlay = 0;
    bool losShowWalls = true;
    std::string losWallColor = "#1a1a1f";
    int losWallOpacity = 82;
    std::string losEvenColor = "#4a4a4f";
    int losEvenOpacity = 60;
    std::string losOddColor = "#2e2e33";
    int losOddOpacity = 68;
    std::string portalColor1 = "#5b21b6";
    std::string portalColor2 = "#7c5cd6";
    std::string portalColor3 = "#a78bfa";
    std::string portalColor4 = "#c4b5fd";
    int portalOpacity = 100;
    bool portalFilled = false;
    int portalThickness = 2;
    std::string closedPortalColor = "#73737f";
    bool closedPortalFilled = false;
    bool portalShowNumber = true;
    std::string portalNumberColor = "#ffffff";
    std::string portalNumberShape = "round";
    int portalNumberOffsetX = 0;
    int portalNumberOffsetY = 0;
    int portalNumberScale = 100;
    bool portalGreyDeleted = false;
    bool portalShowDistance = false;
    std::string connectorColor = "#a78bfa";
    int connectorOpacity = 100;
    int connectorThickness = 3;
    std::string entranceColor = "#fbbf24";
    int entranceOpacity = 50;
    bool entranceFilled = true;
    int entranceThickness = 2;
    bool damageShow = true;
    std::string damageColor = "#0e7490";
    int damageThickness = 6;
    int damageScale = 100;
    bool damageOutline = true;
    std::string damageOutlineColor = "#000000";
    int damageOutlineThickness = 2;
    std::string language = "en";
    int menuX = -1;
    int menuY = -1;
    int collapsedX = -1;
    int collapsedY = -1;
    std::unordered_map<std::string, std::string> keybinds;
    int64_t trackedEntityId = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConfigState, memoryPollRate, gridOverlay, gridColor, gridOpacity, gridMode, gridShowLos, gridShowMove, gridFillOpacity, gridFillColor, backgroundOverlay, losShowWalls, losWallColor, losWallOpacity, losEvenColor, losEvenOpacity, losOddColor, losOddOpacity, portalColor1, portalColor2, portalColor3, portalColor4, portalOpacity, portalFilled, portalThickness, closedPortalColor, closedPortalFilled, portalShowNumber, portalNumberColor, portalNumberShape, portalNumberOffsetX, portalNumberOffsetY, portalNumberScale, portalGreyDeleted, portalShowDistance, connectorColor, connectorOpacity, connectorThickness, entranceColor, entranceOpacity, entranceFilled, entranceThickness, damageShow, damageColor, damageThickness, damageScale, damageOutline, damageOutlineColor, damageOutlineThickness, language, menuX, menuY, collapsedX, collapsedY, keybinds)

class ConfigManager {
private:
    ConfigManager() = default;

    std::string configPath;

public:
    ConfigState state;

    static ConfigManager& get() {
        static ConfigManager instance;
        if (instance.configPath.empty()) {
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            std::string path(buffer);
            size_t pos = path.find_last_of("\\/");
            if (pos != std::string::npos) {
                instance.configPath = path.substr(0, pos) + "\\config.json";
            } else {
                instance.configPath = "config.json";
            }
        }
        return instance;
    }

    void load() {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            Logger::info("No config file found at {}, using defaults", configPath);
            return;
        }

        try {
            nlohmann::json j;
            file >> j;
            state = j.get<ConfigState>();
            Logger::info("Loaded config from {}", configPath);
        } catch (const std::exception& e) {
            Logger::error("Failed to load config: {}", e.what());
        }
    }

    void save() const {
        std::ofstream file(configPath);
        if (!file.is_open()) {
            Logger::error("Failed to open config file for saving: {}", configPath);
            return;
        }

        try {
            nlohmann::json j = state;
            file << j.dump(4);
            Logger::info("Saved config to {}", configPath);
        } catch (const std::exception& e) {
            Logger::error("Failed to save config: {}", e.what());
        }
    }
};
