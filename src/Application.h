#pragma once

#include <thread>
#include "ProcessHelper.h"
#include "AppLogger.h"
#include "Resolve6.hpp"
#include "DataStore.h"
#include "PeriodicAsyncTimer.h"

class Application {
public:
    DataStore store;

    void init() {
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
        run();
    }

private:
    bool _exit = false;
    HWND _gameWindow = nullptr;
    DWORD _gamePid = 0;

    bool retrieveObjectManager() const {
        if (!er6::InitSettings(_gamePid, er6::ManagedBackend::Il2Cpp)) {
            Logger::error("Could not open process or locate UnityPlayer.dll");
            return false;
        }

        er6::WinApiMemoryAccessor mem(er6::g_ctx.process);
        std::uint64_t gomSlotRva = 0;
        if (!er6::FindGomGlobalSlotRvaByScan(mem, er6::g_ctx.unityPlayer.base, er6::g_ctx.gomOff, gomSlotRva)){
            Logger::error("Failed to resolve Unity GameObjectManager RVA");
            return false;
        }

        std::uintptr_t msIdSlotVa = 0;
        if (!er6::FindMsIdToPointerSlotVaByScan(mem, er6::g_ctx.unityPlayer, er6::g_ctx.off, er6::g_ctx.unityPlayerRange, msIdSlotVa, nullptr)){
            Logger::error("Failed to resolve Unity MsIdToPointer table VA");
            return false;
        }

        er6::g_ctx.gomGlobalSlotRva = gomSlotRva;
        er6::g_ctx.gomGlobalSlotVa = er6::g_ctx.unityPlayer.base + static_cast<std::uintptr_t>(gomSlotRva);

        er6::g_ctx.msIdToPointerSlotVa = msIdSlotVa;
        er6::g_ctx.msIdToPointerSlotRva = static_cast<std::uint64_t>(msIdSlotVa - er6::g_ctx.unityPlayer.base);
        return true;
    }

    void run() {
        while (!_exit) {
            if (!IsWindow(_gameWindow))
                break;

            PeriodicAsyncTimer::execute(100, [&]() {
                store.refresh();
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Logger::info("Exit Eliotopy");
    }
};