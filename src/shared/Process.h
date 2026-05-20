#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <string>

class ProcessHelper {
public:
    static DWORD getProcessId(const std::string& name) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        if (Process32First(snap, &pe)) {
            do {
                if (_stricmp(name.c_str(), pe.szExeFile) == 0) {
                    CloseHandle(snap);
                    return pe.th32ProcessID;
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
        return 0;
    }

    struct EnumData { DWORD pid; HWND hwnd; };

    static BOOL CALLBACK enumCallback(HWND hwnd, LPARAM lParam) {
        EnumData& data = *(EnumData*)lParam;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (data.pid == pid && IsWindowVisible(hwnd)) {
            char title[256];
            GetWindowTextA(hwnd, title, sizeof(title));
            if (strlen(title) > 0) {
                data.hwnd = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }

    static HWND getRenderingWindow(DWORD pid) {
        EnumData data = { pid, NULL };
        EnumWindows(enumCallback, (LPARAM)&data);
        return data.hwnd;
    }
};