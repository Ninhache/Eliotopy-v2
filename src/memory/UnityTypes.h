#pragma once
#include <string>
#include <vector>
#include <algorithm>
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

class NativeComponent : public MemoryEntity {
public:
    NativeComponent(uintptr_t addr, const std::source_location& loc = std::source_location::current())
            : MemoryEntity(addr, loc) {
        this->entityName = "NativeComponent";
    }

    Il2CppObject getManagedInstance(const std::source_location& loc = std::source_location::current()) const {
        Logger::debug(loc, "[{}] Retrieving managed instance via GCHandle", entityName);

        uintptr_t gcHandleAddr = readLive<uintptr_t>(UnityOffsets::gcHandle, loc);
        if (gcHandleAddr <= 0x1000) {
            Logger::error(loc, "[{}] Invalid GCHandle pointer (0x{:X})", entityName, gcHandleAddr);
            return Il2CppObject(0);
        }

        Logger::debug(loc, "[{}] Dereferencing GCHandle at 0x{:X}", entityName, gcHandleAddr);
        auto managedAddrOpt = er6::ReadValue<uintptr_t>(gcHandleAddr + UnityOffsets::gcHandleTarget);

        if (!managedAddrOpt.has_value() || managedAddrOpt.value() <= 0x100000) {
            Logger::error(loc, "[{}] Failed to resolve GCHandle target or invalid target address", entityName);
            return Il2CppObject(0);
        }

        Logger::debug(loc, "[{}] Managed instance resolved to 0x{:X}", entityName, managedAddrOpt.value());
        return Il2CppObject(managedAddrOpt.value());
    }
};

class ScriptableObject : public Il2CppObject {
public:
    using Il2CppObject::load;

    ScriptableObject(uintptr_t addr, const std::source_location& loc = std::source_location::current())
            : Il2CppObject(addr, loc) {}
};

class MonoBehaviour : public Il2CppObject {
protected:
    std::string targetGameObjectName;
    std::string targetComponentTypeName;
    bool isNameBased = false;

public:
    using Il2CppObject::load;

    MonoBehaviour(uintptr_t addr, const std::source_location& loc = std::source_location::current())
            : Il2CppObject(addr, loc) {}

    MonoBehaviour(const std::string& gameObjectName, const std::string& componentTypeName, const std::source_location& loc = std::source_location::current())
            : Il2CppObject(0, loc), targetGameObjectName(gameObjectName), targetComponentTypeName(componentTypeName), isNameBased(true) {}

    bool load(const std::source_location& loc = std::source_location::current()) override {
        if (isNameBased && !isValid()) {
            this->entityName = targetComponentTypeName;

            Logger::debug(loc, "[{}] Search GameObject '{}'", entityName, targetGameObjectName);
            auto gameObjects = er6::FindGameObjectThroughName(
                    er6::Mem(), er6::GomGlobalSlotVa(), er6::GomOff(), er6::Off(), targetGameObjectName
            );

            if (gameObjects.empty()) {
                Logger::error(loc, "[{}] FindGameObjectThroughName failed to locate '{}'", entityName, targetGameObjectName);
                return false;
            }

            uintptr_t gameObjectNative = gameObjects[0];
            Logger::debug(loc, "[{}] Found GameObject native at 0x{:X}, searching for component '{}'", entityName, gameObjectNative, targetComponentTypeName);

            uintptr_t componentNativeAddr = er6::GetComponentThroughTypeName(gameObjectNative, targetComponentTypeName.c_str());

            if (componentNativeAddr <= 0x1000) {
                Logger::error(loc, "[{}] GetComponentThroughTypeName failed to locate component '{}'", entityName, targetComponentTypeName);
                return false;
            }

            Logger::debug(loc, "[{}] Found Component native at 0x{:X}", entityName, componentNativeAddr);

            NativeComponent nativeComp(componentNativeAddr, loc);
            Il2CppObject managedInstance = nativeComp.getManagedInstance(loc);

            if (managedInstance.isValid()) {
                this->address = managedInstance.getAddress();
                Logger::debug(loc, "[{}] Successfully hooked MonoBehaviour to 0x{:X}", entityName, this->address);
            } else {
                Logger::error(loc, "[{}] Failed to hook MonoBehaviour, managed instance resolution failed", entityName);
                return false;
            }
        }
        return CachedEntity::load(loc);
    }
};

template <typename TWrapper>
class Il2CppList : public CachedEntity {
private:
    std::vector<TWrapper> items;

public:
    Il2CppList(uintptr_t addr, const std::source_location& loc = std::source_location::current())
            : CachedEntity(addr, loc) {
        this->entityName = getCleanTypeName<Il2CppList<TWrapper>>();
    }

    bool load(const std::source_location& loc = std::source_location::current()) override {
        if (!isValid()) {
            Logger::error(loc, "[{}] Cannot load invalid Il2CppList", entityName);
            return false;
        }

        uintptr_t listHeader[4] = {0};
        Logger::debug(loc, "[{}] Read Il2CppList header (32 bytes) at 0x{:X}", entityName, address);

        if (!er6::Mem().Read(address, listHeader, sizeof(listHeader))) {
            Logger::error(loc, "[{}] Failed to read list header at 0x{:X}", entityName, address);
            return false;
        }

        uintptr_t arrayPtr = *reinterpret_cast<uintptr_t*>((uint8_t*)listHeader + UnityOffsets::listArrayPtr);
        int c = *reinterpret_cast<int*>((uint8_t*)listHeader + UnityOffsets::listSize);

        Logger::debug(loc, "[{}] Parsed list header -> count: {}, arrayPtr: 0x{:X}", entityName, c, arrayPtr);
        items.clear();

        if (c > 0 && arrayPtr > 0x100000) {
            size_t arrayMemorySize = UnityOffsets::listItemsStart + (c * UnityOffsets::pointerSize);
            cache.resize(arrayMemorySize);

            Logger::debug(loc, "[{}] Read {} bytes array from 0x{:X}", entityName, arrayMemorySize, arrayPtr);
            if (!er6::Mem().Read(arrayPtr, cache.data(), arrayMemorySize)) {
                Logger::error(loc, "[{}] Failed to read list items array at 0x{:X}", entityName, arrayPtr);
                return false;
            }

            std::vector<uintptr_t> validPtrs;
            validPtrs.reserve(c);

            for (int i = 0; i < c; ++i) {
                uintptr_t itemAddr = *reinterpret_cast<uintptr_t*>(cache.data() + UnityOffsets::listItemsStart + (i * UnityOffsets::pointerSize));
                if (itemAddr > 0x100000) {
                    validPtrs.push_back(itemAddr);
                } else {
                    Logger::warn(loc, "[{}] List item at index {} has invalid address 0x{:X}", entityName, i, itemAddr);
                }
            }

            if (validPtrs.empty()) return true;

            TWrapper dummy(UINTPTR_MAX);
            size_t expectedItemSize = dummy.getExpectedCacheSize();
            if (expectedItemSize == 0) expectedItemSize = 64;

            std::vector<uintptr_t> sortedPtrs = validPtrs;
            std::sort(sortedPtrs.begin(), sortedPtrs.end());

            struct MemoryChunk {
                uintptr_t minAddr;
                uintptr_t maxAddr;
                std::vector<uint8_t> buffer;
            };
            std::vector<MemoryChunk> chunks;

            uintptr_t currentMin = sortedPtrs[0];
            uintptr_t currentMax = sortedPtrs[0];
            const size_t MAX_GAP = 512 * 1024;

            for (size_t i = 1; i < sortedPtrs.size(); ++i) {
                if (sortedPtrs[i] - currentMax > MAX_GAP) {
                    chunks.push_back({currentMin, currentMax + expectedItemSize, {}});
                    currentMin = sortedPtrs[i];
                }
                currentMax = sortedPtrs[i];
            }
            chunks.push_back({currentMin, currentMax + expectedItemSize, {}});

            int successfulChunks = 0;
            size_t totalBytesRead = 0;

            for (auto& chunk : chunks) {
                size_t span = chunk.maxAddr - chunk.minAddr;
                chunk.buffer.resize(span);
                if (er6::Mem().Read(chunk.minAddr, chunk.buffer.data(), span)) {
                    successfulChunks++;
                    totalBytesRead += span;
                } else {
                    chunk.buffer.clear();
                }
            }

            Logger::debug(loc, "[{}] Smart Batch Read: Fetched {}/{} chunks (total {} bytes) for {} items", entityName, successfulChunks, chunks.size(), totalBytesRead, validPtrs.size());

            items.reserve(validPtrs.size());
            for (uintptr_t itemAddr : validPtrs) {
                TWrapper item(itemAddr);
                item.setName(getCleanTypeName<TWrapper>());

                bool injected = false;
                for (const auto& chunk : chunks) {
                    if (!chunk.buffer.empty() && itemAddr >= chunk.minAddr && itemAddr < chunk.maxAddr) {
                        size_t offset = itemAddr - chunk.minAddr;
                        item.injectCache(chunk.buffer.data() + offset, expectedItemSize);
                        injected = true;
                        break;
                    }
                }

                if (!injected) {
                    item.load(loc);
                }

                items.push_back(std::move(item));
            }

            Logger::debug(loc, "[{}] Successfully populated {} wrappers from chunks", entityName, items.size());
            return true;

        } else if (c > 0) {
            Logger::warn(loc, "[{}] List has count {} but arrayPtr is invalid (0x{:X})", entityName, c, arrayPtr);
            return false;
        }

        return true;
    }

    std::vector<TWrapper> getAll() const {
        return items;
    }

    int count() const {
        return items.size();
    }
};