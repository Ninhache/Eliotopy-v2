#pragma once
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#include <string>
#include <fstream>
#include "json.hpp"
#include <atomic>
#include "AppLogger.h"

inline std::atomic<int> g_memoryPollRate{250};

struct ConfigState {
    bool gridOverlay = true;
    std::string gridColor = "#ffffff";
    std::string gridMode = "rp";
    bool gridShowLos = true;
    bool gridShowMove = false;
    int gridFillOpacity = 60;
    std::string gridFillColor = "#555555";
    int backgroundOverlay = 0;
    int menuX = -1;
    int menuY = -1;
    int64_t trackedEntityId = 0;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConfigState, gridOverlay, gridColor, gridMode, gridShowLos, gridShowMove, gridFillOpacity, gridFillColor, backgroundOverlay, menuX, menuY)

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
