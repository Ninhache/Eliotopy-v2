#pragma once
#include <vector>
#include <algorithm>
#include <source_location>
#include "Memory.h"
#include "Offsets.h"

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
            const size_t MAX_GAP = 4096;

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
