#pragma once
#include <vector>
#include <cstdint>
#include <source_location>
#include "MemoryEntity.h"
#include "MemoryUtils.h"

class CachedEntity : public MemoryEntity {
protected:
    std::vector<uint8_t> cache;
    size_t baseSize = 0;
    size_t dynamicSize = 0;

public:
    CachedEntity(uintptr_t addr, const std::source_location& loc = std::source_location::current())
            : MemoryEntity(addr, loc) {}

    CachedEntity(const CachedEntity& other)
            : MemoryEntity(other), cache(other.cache), baseSize(other.baseSize), dynamicSize(other.dynamicSize) {
        g_currentCopyTarget = this;
    }

    CachedEntity(CachedEntity&& other) noexcept
            : MemoryEntity(std::move(other)), cache(std::move(other.cache)), baseSize(other.baseSize), dynamicSize(other.dynamicSize) {
        g_currentCopyTarget = this;
    }

    CachedEntity& operator=(const CachedEntity& other) {
        if (this != &other) {
            MemoryEntity::operator=(other);
            cache = other.cache;
            baseSize = other.baseSize;
            dynamicSize = other.dynamicSize;
            g_currentCopyTarget = this;
        }
        return *this;
    }

    CachedEntity& operator=(CachedEntity&& other) noexcept {
        if (this != &other) {
            MemoryEntity::operator=(std::move(other));
            cache = std::move(other.cache);
            baseSize = other.baseSize;
            dynamicSize = other.dynamicSize;
            g_currentCopyTarget = this;
        }
        return *this;
    }

    void registerField(size_t offset, size_t size) {
        if (offset + size > baseSize) {
            baseSize = offset + size;
        }
    }

    void addDynamicSize(size_t size) {
        dynamicSize += size;
    }

    size_t getExpectedCacheSize() const {
        return baseSize + dynamicSize;
    }

    void injectCache(const uint8_t* rawData, size_t size) {
        cache.assign(rawData, rawData + size);
    }

    virtual bool load(const std::source_location& loc = std::source_location::current()) {
        size_t totalSize = baseSize + dynamicSize;
        if (totalSize == 0) {
            Logger::warn(loc, "[{}] Load aborted, calculated size is 0", entityName);
            return false;
        }

        if (!isValid()) {
            Logger::error(loc, "[{}] Cannot load cache, entity address is invalid (0x{:X})", entityName, address);
            return false;
        }

        Logger::debug(loc, "[{}] Read caching {} bytes from 0x{:X}", entityName, totalSize, address);
        cache.resize(totalSize);

        if (!er6::Mem().Read(address, cache.data(), totalSize)) {
            Logger::error(loc, "[{}] Cache load failed at 0x{:X}", entityName, address);
            cache.clear();
            return false;
        }
        Logger::debug(loc, "[{}] Cache successfully populated", entityName);
        return true;
    }

    template<typename T>
    T readCached(size_t offset, const std::source_location& loc = std::source_location::current()) const {
        if (offset + sizeof(T) <= cache.size()) {
            return *reinterpret_cast<const T*>(cache.data() + offset);
        }
        Logger::debug(loc, "[{}] Cache miss for offset 0x{:X}, falling back to live read", entityName, offset);
        return readLive<T>(offset, loc);
    }
};
