#pragma once
#include <source_location>
#include "Memory.h"
#include "Offsets.h"

class Il2CppObject : public CachedEntity {
public:
    using CachedEntity::load;

    Il2CppObject(uintptr_t addr, const std::source_location& loc = std::source_location::current())
            : CachedEntity(addr, loc) {}

    bool verifyCachedPtr(uintptr_t expectedNativeAddr, const std::source_location& loc = std::source_location::current()) const {
        Logger::debug(loc, "[{}] Verifying cached ptr against 0x{:X}", entityName, expectedNativeAddr);
        uintptr_t actual = readLive<uintptr_t>(UnityOffsets::cachedPtr, loc);
        bool match = (actual == expectedNativeAddr);
        if (!match) {
            Logger::warn(loc, "[{}] Cached ptr verification failed: expected 0x{:X}, got 0x{:X}", entityName, expectedNativeAddr, actual);
        }
        return match;
    }

    bool load(uintptr_t expectedNativeAddr, const std::source_location& loc = std::source_location::current()) {
        if (!CachedEntity::load(loc)) return false;
        return verifyCachedPtr(expectedNativeAddr, loc);
    }
};
