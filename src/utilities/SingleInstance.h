#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <cstdint>
#include "ModuleSignature.h"

class SingleInstance {
public:
    static bool acquire() {
        std::wstring self = selfPath();
        if (self.empty())
            return true;

        uint64_t selfSize = ModuleSignature::fileSize(self);
        if (selfSize == 0)
            return true;

        DWORD selfPid = GetCurrentProcessId();
        uint64_t selfCreation = creationTime(GetCurrentProcess());
        uint64_t selfHash = 0;
        bool selfHashReady = false;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return true;

        bool yield = false;
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (entry.th32ProcessID == selfPid || entry.th32ProcessID == 0)
                    continue;

                HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
                if (!process)
                    continue;

                std::wstring path = imagePath(process);
                if (path.empty() || ModuleSignature::fileSize(path) != selfSize) {
                    CloseHandle(process);
                    continue;
                }
                if (!selfHashReady) {
                    selfHash = ModuleSignature::contentHash(self);
                    selfHashReady = true;
                }
                if (ModuleSignature::contentHash(path) != selfHash) {
                    CloseHandle(process);
                    continue;
                }

                uint64_t creation = creationTime(process);
                CloseHandle(process);

                bool startedEarlier = (creation != 0 && selfCreation != 0)
                    ? (creation < selfCreation || (creation == selfCreation && entry.th32ProcessID < selfPid))
                    : (entry.th32ProcessID < selfPid);
                if (startedEarlier) {
                    yield = true;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return !yield;
    }

private:
    static std::wstring selfPath() {
        wchar_t buffer[MAX_PATH] = {};
        DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::wstring(buffer, length);
    }

    static std::wstring imagePath(HANDLE process) {
        wchar_t buffer[MAX_PATH] = {};
        DWORD size = MAX_PATH;
        std::wstring result;
        if (QueryFullProcessImageNameW(process, 0, buffer, &size))
            result.assign(buffer, size);
        return result;
    }

    static uint64_t creationTime(HANDLE process) {
        FILETIME created{}, ended{}, kernelTime{}, userTime{};
        if (!GetProcessTimes(process, &created, &ended, &kernelTime, &userTime))
            return 0;
        return (static_cast<uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
    }
};
