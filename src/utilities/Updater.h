#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <fstream>
#include <thread>
#include <cctype>
#include <cstdlib>
#include "json.hpp"
#include "Utils.h"
#include "Version.h"

#pragma comment(lib, "winhttp.lib")

class Updater {
public:
    static void cleanupOld() {
        std::wstring oldPath = selfPath() + L".old";
        if (GetFileAttributesW(oldPath.c_str()) == INVALID_FILE_ATTRIBUTES)
            return;
        std::thread([oldPath]() {
            for (int attempt = 0; attempt < 50; ++attempt) {
                if (DeleteFileW(oldPath.c_str()) || GetFileAttributesW(oldPath.c_str()) == INVALID_FILE_ATTRIBUTES)
                    return;
                Sleep(200);
            }
        }).detach();
    }

    static void run() {
        std::optional<Release> latest = check();
        if (!latest)
            return;

        std::string prompt = "A new version of Eliotopy is available.\n\n"
                             "Installed: " ELIOTOPY_VERSION "\nLatest: " + latest->version +
                             "\n\nUpdate now?";
        if (MessageBoxA(nullptr, prompt.c_str(), "Eliotopy", MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST) != IDYES)
            return;

        std::vector<char> binary;
        if (!httpGet(Utils::toWide(latest->downloadUrl), binary) || binary.empty()) {
            MessageBoxA(nullptr, "Download failed. Please try again later.", "Eliotopy", MB_OK | MB_ICONWARNING | MB_TOPMOST);
            return;
        }

        std::wstring self = selfPath();
        std::wstring newPath = self + L".new";
        std::wstring oldPath = self + L".old";

        if (!writeFile(newPath, binary)) {
            MessageBoxA(nullptr, "Could not write the update.", "Eliotopy", MB_OK | MB_ICONWARNING | MB_TOPMOST);
            return;
        }

        DeleteFileW(oldPath.c_str());
        if (!MoveFileW(self.c_str(), oldPath.c_str()) || !MoveFileW(newPath.c_str(), self.c_str())) {
            MoveFileW(oldPath.c_str(), self.c_str());
            DeleteFileW(newPath.c_str());
            MessageBoxA(nullptr, "Could not apply the update.", "Eliotopy", MB_OK | MB_ICONWARNING | MB_TOPMOST);
            return;
        }

        relaunch(self);
        ExitProcess(0);
    }

private:
    struct Release {
        std::string version;
        std::string downloadUrl;
    };

    static std::optional<Release> check() {
        std::vector<char> body;
        if (!httpGet(L"https://api.github.com/repos/Romain-P/Eliotopy-v2/releases/latest", body) || body.empty())
            return std::nullopt;

        try {
            nlohmann::json j = nlohmann::json::parse(std::string(body.begin(), body.end()));
            std::string tag = j.value("tag_name", "");
            if (tag.empty())
                return std::nullopt;

            std::string version = (tag[0] == 'v' || tag[0] == 'V') ? tag.substr(1) : tag;
            if (!isNewer(version, ELIOTOPY_VERSION))
                return std::nullopt;

            for (const auto& asset : j.value("assets", nlohmann::json::array())) {
                std::string name = asset.value("name", "");
                if (name.size() >= 4 && name.compare(name.size() - 4, 4, ".exe") == 0) {
                    std::string url = asset.value("browser_download_url", "");
                    if (!url.empty())
                        return Release{ version, url };
                }
            }
        } catch (...) {
        }
        return std::nullopt;
    }

    static std::array<int, 3> parseVersion(const std::string& s) {
        std::array<int, 3> out{ 0, 0, 0 };
        int index = 0;
        std::string part;
        for (char c : s) {
            if (c == '.') {
                if (index < 3) out[index++] = std::atoi(part.c_str());
                part.clear();
            } else if (std::isdigit(static_cast<unsigned char>(c))) {
                part += c;
            } else {
                break;
            }
        }
        if (index < 3 && !part.empty())
            out[index] = std::atoi(part.c_str());
        return out;
    }

    static bool isNewer(const std::string& candidate, const std::string& current) {
        return parseVersion(candidate) > parseVersion(current);
    }

    static std::wstring selfPath() {
        wchar_t buffer[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        return buffer;
    }

    static bool writeFile(const std::wstring& path, const std::vector<char>& data) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        return file.good();
    }

    static void relaunch(const std::wstring& path) {
        std::wstring command = L"\"" + path + L"\" /u";
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }

    static bool httpGet(const std::wstring& url, std::vector<char>& out) {
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        wchar_t host[256] = {};
        wchar_t resource[4096] = {};
        parts.lpszHostName = host;
        parts.dwHostNameLength = ARRAYSIZE(host) - 1;
        parts.lpszUrlPath = resource;
        parts.dwUrlPathLength = ARRAYSIZE(resource) - 1;
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts))
            return false;

        HINTERNET session = WinHttpOpen(L"Eliotopy-Updater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session)
            return false;
        WinHttpSetTimeouts(session, 4000, 4000, 8000, 30000);

        bool ok = false;
        HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
        if (connection) {
            DWORD flags = (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET request = WinHttpOpenRequest(connection, L"GET", resource, nullptr,
                                                   WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (request) {
                if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(request, nullptr)) {
                    DWORD status = 0, statusSize = sizeof(status);
                    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
                    if (status == 200) {
                        DWORD available = 0;
                        do {
                            available = 0;
                            if (!WinHttpQueryDataAvailable(request, &available))
                                break;
                            if (available == 0)
                                break;
                            size_t offset = out.size();
                            out.resize(offset + available);
                            DWORD read = 0;
                            if (!WinHttpReadData(request, out.data() + offset, available, &read))
                                break;
                            out.resize(offset + read);
                        } while (available > 0);
                        ok = true;
                    }
                }
                WinHttpCloseHandle(request);
            }
            WinHttpCloseHandle(connection);
        }
        WinHttpCloseHandle(session);
        return ok;
    }
};
