#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include <source_location>
#include "Resolve6.hpp"
#include "AppLogger.h"

inline thread_local void* g_currentCopyTarget = nullptr;

template<typename T>
static std::string getCleanTypeName() {
    std::string name = typeid(T).name();
    size_t pos;
    while ((pos = name.find("class ")) != std::string::npos) name.erase(pos, 6);
    while ((pos = name.find("struct ")) != std::string::npos) name.erase(pos, 7);
    return name;
}

class MemoryEntity {
protected:
    uintptr_t address;
    std::string entityName;

    static std::string extractEntityName(const std::source_location& loc) {
        std::string_view file = loc.file_name();
        size_t lastSlash = file.find_last_of("/\\");
        if (lastSlash != std::string_view::npos) file = file.substr(lastSlash + 1);
        size_t dot = file.find_last_of('.');
        if (dot != std::string_view::npos) file = file.substr(0, dot);
        return std::string(file);
    }

public:
    MemoryEntity(uintptr_t addr, const std::source_location& loc = std::source_location::current())
            : address(addr), entityName(extractEntityName(loc)) {}

    virtual ~MemoryEntity() = default;

    bool isValid() const { return address > 0x100000 && address != UINTPTR_MAX; }
    uintptr_t getAddress() const { return address; }
    const std::string& getName() const { return entityName; }
    void setName(std::string_view name) { entityName = name; }

    template<typename T>
    T readLive(size_t offset, const std::source_location& loc = std::source_location::current()) const {
        if (!isValid()) {
            Logger::error(loc, "[{}] Cannot read live memory, entity address is invalid (0x{:X})", entityName, address);
            return T{};
        }

        Logger::debug(loc, "[{}] Read {} bytes at 0x{:X} (offset 0x{:X})", entityName, sizeof(T), address + offset, offset);
        auto result = er6::ReadValue<T>(address + offset);

        if (!result.has_value()) {
            Logger::error(loc, "[{}] Memory read failed at 0x{:X}", entityName, address + offset);
            return T{};
        }
        return result.value();
    }
};

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

template <typename T, size_t Offset>
class Field {
private:
    CachedEntity* parent;
public:
    Field(CachedEntity* p) : parent(p) {
        if (parent) parent->registerField(Offset, sizeof(T));
    }

    Field(const Field& other) {
        parent = g_currentCopyTarget ? reinterpret_cast<CachedEntity*>(g_currentCopyTarget) : other.parent;
        if (parent) parent->registerField(Offset, sizeof(T));
    }

    Field(Field&& other) noexcept {
        parent = g_currentCopyTarget ? reinterpret_cast<CachedEntity*>(g_currentCopyTarget) : other.parent;
        if (parent) parent->registerField(Offset, sizeof(T));
    }

    Field& operator=(const Field& other) {
        if (g_currentCopyTarget) parent = reinterpret_cast<CachedEntity*>(g_currentCopyTarget);
        return *this;
    }

    T get(const std::source_location& loc = std::source_location::current()) const {
        if (!parent || !parent->isValid()) {
            Logger::warn(loc, "[{}] Invalid parent for field read at offset 0x{:X}", parent ? parent->getName() : "Unknown", Offset);
            return T{};
        }
        return parent->readCached<T>(Offset, loc);
    }

    operator T() const { return get(); }
};

template <typename TWrapper, size_t Offset>
class RemotePtr {
private:
    CachedEntity* parent;
public:
    RemotePtr(CachedEntity* p) : parent(p) {
        if (parent) parent->registerField(Offset, sizeof(uintptr_t));
    }

    RemotePtr(const RemotePtr& other) {
        parent = g_currentCopyTarget ? reinterpret_cast<CachedEntity*>(g_currentCopyTarget) : other.parent;
        if (parent) parent->registerField(Offset, sizeof(uintptr_t));
    }

    RemotePtr(RemotePtr&& other) noexcept {
        parent = g_currentCopyTarget ? reinterpret_cast<CachedEntity*>(g_currentCopyTarget) : other.parent;
        if (parent) parent->registerField(Offset, sizeof(uintptr_t));
    }

    RemotePtr& operator=(const RemotePtr& other) {
        if (g_currentCopyTarget) parent = reinterpret_cast<CachedEntity*>(g_currentCopyTarget);
        return *this;
    }

    TWrapper get(const std::source_location& loc = std::source_location::current()) const {
        if (!parent || !parent->isValid()) {
            Logger::warn(loc, "[{}] Invalid parent for RemotePtr evaluation at offset 0x{:X}", parent ? parent->getName() : "Unknown", Offset);
            return TWrapper(0);
        }

        uintptr_t ptrAddr = parent->readCached<uintptr_t>(Offset, loc);

        if (ptrAddr <= 0x100000) {
            Logger::warn(loc, "[{}] RemotePtr at offset 0x{:X} is null (0x{:X})", parent->getName(), Offset, ptrAddr);
            return TWrapper(0);
        }

        Logger::debug(loc, "[{}] Resolved RemotePtr at offset 0x{:X} -> 0x{:X}", parent->getName(), Offset, ptrAddr);
        TWrapper wrapper(ptrAddr);
        wrapper.setName(getCleanTypeName<TWrapper>());

        wrapper.load(loc);

        return wrapper;
    }

    TWrapper operator->() const {
        return get();
    }

    bool isNull(const std::source_location& loc = std::source_location::current()) const {
        if (!parent || !parent->isValid()) return true;
        return parent->readCached<uintptr_t>(Offset, loc) <= 0x100000;
    }
};