#pragma once
#include <string>
#include <vector>
#include <source_location>
#include "Memory.h"
#include "Offsets.h"

class ManagedString : public CachedEntity {
private:
    std::string _decoded;

    static std::string utf16ToUtf8(const char16_t* data, int32_t len) {
        if (!data || len <= 0) return {};
        std::string result;
        result.reserve(static_cast<size_t>(len));
        for (int32_t i = 0; i < len; ++i) {
            char16_t c = data[i];
            if (c < 0x80) {
                result.push_back(static_cast<char>(c));
            } else if (c < 0x800) {
                result.push_back(static_cast<char>(0xC0 | (c >> 6)));
                result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            } else {
                result.push_back(static_cast<char>(0xE0 | (c >> 12)));
                result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
            }
        }
        return result;
    }

public:
    ManagedString(uintptr_t addr, const std::source_location& loc = std::source_location::current())
        : CachedEntity(addr, loc) {
        this->entityName = "ManagedString";
    }

    bool load(const std::source_location& loc = std::source_location::current()) override {
        _decoded.clear();
        if (!isValid()) {
            Logger::warn(loc, "[ManagedString] Invalid address 0x{:X}", address);
            return false;
        }

        auto lenOpt = er6::ReadValue<int32_t>(address + ManagedStringOffsets::stringLength);
        if (!lenOpt.has_value() || lenOpt.value() <= 0 || lenOpt.value() > 65535) {
            Logger::warn(loc, "[ManagedString] Invalid or empty length at 0x{:X}", address + ManagedStringOffsets::stringLength);
            return false;
        }
        int32_t strLen = lenOpt.value();

        std::vector<char16_t> utf16buf(strLen);
        if (!er6::Mem().Read(address + ManagedStringOffsets::firstChar, utf16buf.data(), static_cast<size_t>(strLen) * sizeof(char16_t))) {
            Logger::error(loc, "[ManagedString] Failed to read UTF-16 buffer at 0x{:X} ({} chars)", address + ManagedStringOffsets::firstChar, strLen);
            return false;
        }

        _decoded = utf16ToUtf8(utf16buf.data(), strLen);
        Logger::debug(loc, "[ManagedString] Decoded '{}' ({} chars)", _decoded, strLen);
        return true;
    }

    const std::string& str() const { return _decoded; }
    const char* c_str() const { return _decoded.c_str(); }
    bool empty() const { return _decoded.empty(); }
};
