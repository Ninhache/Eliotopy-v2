#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include "shared/ProcessHelper.h"
#include "shared/AppLogger.h"
#include "Resolve6.hpp"
#include "DataStore.h"
#include "Keybind.h"
#include "shared/Config.h"
#include "overlay/Renderer.h"
#include "overlay/WebViewPanel.h"
#include "shared/PeriodicAsyncTimer.h"
#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

class Application {
public:
    DataStore store;
    Renderer renderer;
    WebViewPanel panel;
    KeybindManager keybinds;

    void init() {
        ConfigManager::get().load();

        Logger::info("Searching for Dofus.exe");
        _gamePid = ProcessHelper::getProcessId("Dofus.exe");
        if (_gamePid == 0) {
            MessageBoxA(NULL, "Please start dofus first", "Error", MB_OK | MB_ICONERROR);
            return;
        }
        _gameWindow = ProcessHelper::getRenderingWindow(_gamePid);
        if (_gameWindow == nullptr) {
            MessageBoxA(NULL, "Could not find dofus rendering window", "Error", MB_OK | MB_ICONERROR);
            return;
        }
        Logger::info("Found Dofus.exe [pid={}]", _gamePid);

        if (!retrieveObjectManager()) {
            MessageBoxA(NULL, "Could not locate GameObjectManager", "Error", MB_OK | MB_ICONERROR);
            return;
        }

        Logger::info("Located GameObjectManager successfully");
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        renderer.init(_gameWindow);
        panel.init(_gameWindow);
        keybinds.init(_gameWindow);
        registerKeybinds();
        run();
    }

private:
    std::atomic<bool> _exit = false;
    HWND _gameWindow = nullptr;
    DWORD _gamePid = 0;

    std::mutex _stateMutex;
    GameState _sharedStates;

    bool retrieveObjectManager() const {
        if (!er6::InitSettings(_gamePid, er6::ManagedBackend::Il2Cpp)) {
            Logger::error("Could not open process or locate UnityPlayer.dll");
            return false;
        }

        er6::WinApiMemoryAccessor mem(er6::g_ctx.process);
        std::uint64_t gomSlotRva = 0;
        if (!er6::FindGomGlobalSlotRvaByScan(mem, er6::g_ctx.unityPlayer.base, er6::g_ctx.gomOff, gomSlotRva)) {
            Logger::error("Failed to resolve Unity GameObjectManager RVA");
            return false;
        }

        std::uintptr_t msIdSlotVa = 0;
        if (!er6::FindMsIdToPointerSlotVaByScan(mem, er6::g_ctx.unityPlayer, er6::g_ctx.off, er6::g_ctx.unityPlayerRange, msIdSlotVa, nullptr)) {
            Logger::error("Failed to resolve Unity MsIdToPointer table VA");
            return false;
        }

        er6::g_ctx.gomGlobalSlotRva = gomSlotRva;
        er6::g_ctx.gomGlobalSlotVa = er6::g_ctx.unityPlayer.base + static_cast<std::uintptr_t>(gomSlotRva);

        er6::g_ctx.msIdToPointerSlotVa = msIdSlotVa;
        er6::g_ctx.msIdToPointerSlotRva = static_cast<std::uint64_t>(msIdSlotVa - er6::g_ctx.unityPlayer.base);
        return true;
    }

    void registerKeybinds() {
        auto& registry = KeybindRegistry::get();
        registry.attach(&keybinds);

        registry.registerAction("toggle_grid", "Toggle Grid", Keybind{ 'G', false, true, false }, [] {
            auto& config = ConfigManager::get();
            config.state.gridOverlay = !config.state.gridOverlay;
            config.save();
        });

        registry.registerAction("toggle_menu", "Toggle Menu", Keybind{}, [this] {
            panel.toggleCollapse();
        });

        registry.registerHoldAction("los_assist", "LOS Assist", Keybind{},
            [] { g_losAssistActive = true; },
            [] { g_losAssistActive = false; });

        registry.loadFromConfig();
    }

    void run() {
        timeBeginPeriod(1);

        std::thread memoryThread([this]() {
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
            while (!_exit) {
                if (!IsWindow(_gameWindow)) {
                    _exit = true;
                    break;
                }

                store.refresh();

                {
                    std::lock_guard<std::mutex> lock(_stateMutex);
                    _sharedStates = store.states;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(g_memoryPollRate.load()));
            }
        });

        while (!_exit) {
            if (!IsWindow(_gameWindow)) {
                _exit = true;
                break;
            }

            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    _exit = true;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            GameState localStates;
            {
                std::lock_guard<std::mutex> lock(_stateMutex);
                localStates = _sharedStates;
            }

            renderer.update(localStates);
            panel.update(localStates);

            MsgWaitForMultipleObjects(0, NULL, FALSE, 16, QS_ALLINPUT);
        }

        keybinds.shutdown();

        if (memoryThread.joinable())
            memoryThread.join();

        Logger::info("Exit Eliotopy");
    }
};