#pragma once
#include <windows.h>
#include <string>
#include <cwctype>
#include "Utils.h"

class StealthName {
public:
    static void enforce() {
        std::wstring path = selfPath();
        if (path.empty())
            return;

        if (!reveals(lower(fileName(path))))
            return;

        size_t slash = path.find_last_of(L"\\/");
        std::wstring directory = (slash == std::wstring::npos) ? L"." : path.substr(0, slash);
        std::wstring target = uniqueName(directory);
        if (target.empty())
            return;

        if (!MoveFileW(path.c_str(), target.c_str()))
            return;

        if (relaunch(target))
            ExitProcess(0);
    }

private:
    static std::wstring selfPath() {
        wchar_t buffer[MAX_PATH] = {};
        DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return std::wstring(buffer, length);
    }

    static std::wstring fileName(const std::wstring& path) {
        size_t slash = path.find_last_of(L"\\/");
        return (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    }

    static std::wstring lower(std::wstring text) {
        for (wchar_t& c : text)
            c = static_cast<wchar_t>(towlower(c));
        return text;
    }

    static bool reveals(const std::wstring& lowered) {
        static const std::wstring brand = L"eliotopy";
        const size_t minChunk = 4;
        for (size_t length = minChunk; length <= brand.size(); ++length)
            for (size_t start = 0; start + length <= brand.size(); ++start)
                if (lowered.find(brand.substr(start, length)) != std::wstring::npos)
                    return true;
        return false;
    }

    static std::wstring uniqueName(const std::wstring& directory) {
        for (int attempt = 0; attempt < 32; ++attempt) {
            std::wstring name = Utils::toWide(Utils::randomString(10));
            if (reveals(lower(name)))
                continue;
            std::wstring candidate = directory + L"\\" + name + L".exe";
            if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES)
                return candidate;
        }
        return L"";
    }

    static bool relaunch(const std::wstring& path) {
        std::wstring command = L"\"" + path + L"\" /u";
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
            return false;
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }
};
