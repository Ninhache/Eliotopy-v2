#pragma once
#include <windows.h>
#include <string>
#include <cstdint>
#include <fstream>

namespace ModuleSignature {
    inline uint64_t fileSize(const std::wstring& path) {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
            return 0;
        return (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
    }

    inline uint64_t contentHash(const std::wstring& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            return 0;

        uint64_t hash = 1469598103934665603ull;
        char buffer[65536];
        while (file) {
            file.read(buffer, sizeof(buffer));
            std::streamsize count = file.gcount();
            for (std::streamsize i = 0; i < count; ++i) {
                hash ^= static_cast<unsigned char>(buffer[i]);
                hash *= 1099511628211ull;
            }
        }
        return hash;
    }
}
