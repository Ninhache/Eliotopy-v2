#pragma once
#include <map>
#include <unordered_map>
#include <vector>
#include <source_location>
#include "Memory.h"
#include "Offsets.h"

template<typename TKey, typename TValue>
class ManagedMap : public CachedEntity {
private:
    static constexpr bool isPrimitive = !std::is_base_of_v<CachedEntity, TValue>;

    struct Entry {
        TKey key;
        std::conditional_t<isPrimitive, TValue, uintptr_t> value;
    };

    std::vector<Entry> _entries;

public:
    ManagedMap(uintptr_t addr, const std::source_location& loc = std::source_location::current())
        : CachedEntity(addr, loc) {
        this->entityName = "ManagedMap";
    }

    bool load(const std::source_location& loc = std::source_location::current()) override {
        _entries.clear();
        if (!isValid()) return false;

        auto entriesOpt = er6::ReadValue<uintptr_t>(address + ManagedMapOffsets::dictEntriesArray);
        if (!entriesOpt.has_value() || entriesOpt.value() <= 0x100000) return false;
        uintptr_t entAddr = entriesOpt.value();

        auto lengthOpt = er6::ReadValue<int32_t>(entAddr + ManagedMapOffsets::arrayLength);
        if (!lengthOpt.has_value() || lengthOpt.value() <= 0 || lengthOpt.value() > 1024) return false;
        int32_t length = lengthOpt.value();

        for (int32_t i = 0; i < length; ++i) {
            uintptr_t base = entAddr + ManagedMapOffsets::arrayData + static_cast<uintptr_t>(i) * ManagedMapOffsets::entrySize;

            auto hashCode = er6::ReadValue<int32_t>(base + ManagedMapOffsets::entryHashCode);
            if (!hashCode.has_value() || hashCode.value() < 0) continue;

            auto key = er6::ReadValue<TKey>(base + ManagedMapOffsets::entryKey);
            if (!key.has_value()) continue;

            if constexpr (isPrimitive) {
                auto val = er6::ReadValue<TValue>(base + ManagedMapOffsets::entryValue);
                if (val.has_value()) {
                    _entries.push_back({key.value(), val.value()});
                }
            } else {
                auto valPtr = er6::ReadValue<uintptr_t>(base + ManagedMapOffsets::entryValue);
                if (valPtr.has_value() && valPtr.value() > 0x100000) {
                    _entries.push_back({key.value(), valPtr.value()});
                }
            }
        }

        Logger::debug(loc, "[ManagedMap] Parsed {} entries from 0x{:X}", _entries.size(), address);
        return true;
    }

    TValue getValueAt(size_t index, const std::source_location& loc = std::source_location::current()) const {
        if constexpr (isPrimitive) {
            if (index >= _entries.size()) return TValue{};
            return _entries[index].value;
        } else {
            if (index >= _entries.size()) return TValue(0);
            TValue val(_entries[index].value);
            val.setName(getCleanTypeName<TValue>());
            val.load(loc);
            return val;
        }
    }

    TValue getValueByKey(const TKey& key, const std::source_location& loc = std::source_location::current()) const {
        for (const auto& e : _entries) {
            if (e.key == key) {
                if constexpr (isPrimitive) {
                    return e.value;
                } else {
                    TValue val(e.value);
                    val.setName(getCleanTypeName<TValue>());
                    val.load(loc);
                    return val;
                }
            }
        }
        if constexpr (isPrimitive) return TValue{};
        else return TValue(0);
    }

    size_t count() const { return _entries.size(); }

    std::map<TKey, TValue> toOrderedMap(const std::source_location& loc = std::source_location::current()) const {
        std::map<TKey, TValue> result;
        for (const auto& e : _entries) {
            if constexpr (isPrimitive) {
                result.insert({e.key, e.value});
            } else {
                TValue val(e.value);
                val.setName(getCleanTypeName<TValue>());
                val.load(loc);
                result.insert({e.key, std::move(val)});
            }
        }
        return result;
    }

    std::unordered_map<TKey, TValue> toUnorderedMap(const std::source_location& loc = std::source_location::current()) const {
        std::unordered_map<TKey, TValue> result;
        result.reserve(_entries.size());
        for (const auto& e : _entries) {
            if constexpr (isPrimitive) {
                result.insert({e.key, e.value});
            } else {
                TValue val(e.value);
                val.setName(getCleanTypeName<TValue>());
                val.load(loc);
                result.insert({e.key, std::move(val)});
            }
        }
        return result;
    }
};
