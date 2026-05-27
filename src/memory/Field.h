#pragma once
#include <cstdint>
#include <source_location>
#include <type_traits>
#include "CachedEntity.h"
#include "MemoryUtils.h"

template<typename T, size_t... Offsets>
class Field {
private:
    CachedEntity* parent;
    static constexpr size_t offsets[] = { Offsets... };
    static constexpr bool isPrimitive = !std::is_base_of_v<CachedEntity, T>;
    static constexpr size_t numOffsets = sizeof...(Offsets);
    static_assert(numOffsets >= 1, "Field requires at least one offset");

    void registerSelf() {
        if (!parent) return;
        if constexpr (isPrimitive && numOffsets == 1)
            parent->registerField(offsets[0], sizeof(T));
        else
            parent->registerField(offsets[0], sizeof(uintptr_t));
    }

public:
    Field(CachedEntity* p) : parent(p) { registerSelf(); }

    Field(const Field& other) {
        parent = g_currentCopyTarget ? reinterpret_cast<CachedEntity*>(g_currentCopyTarget) : other.parent;
        registerSelf();
    }

    Field(Field&& other) noexcept {
        parent = g_currentCopyTarget ? reinterpret_cast<CachedEntity*>(g_currentCopyTarget) : other.parent;
        registerSelf();
    }

    Field& operator=(const Field& other) {
        if (g_currentCopyTarget) parent = reinterpret_cast<CachedEntity*>(g_currentCopyTarget);
        return *this;
    }

    T get(const std::source_location& loc = std::source_location::current()) const {
        if (!parent || !parent->isValid()) {
            if constexpr (isPrimitive) return T{};
            else return T(0);
        }

        if constexpr (isPrimitive && numOffsets == 1) {
            return parent->readCached<T>(offsets[0], loc);
        } else {
            uintptr_t current = parent->readCached<uintptr_t>(offsets[0], loc);

            constexpr size_t ptrSteps = isPrimitive ? numOffsets - 1 : numOffsets;
            for (size_t i = 1; i < ptrSteps; ++i) {
                if (current <= 0x100000) {
                    if constexpr (isPrimitive) return T{};
                    else return T(0);
                }
                auto next = er6::ReadValue<uintptr_t>(current + offsets[i]);
                if (!next.has_value()) {
                    if constexpr (isPrimitive) return T{};
                    else return T(0);
                }
                current = next.value();
            }

            if (current <= 0x100000) {
                if constexpr (isPrimitive) return T{};
                else return T(0);
            }

            if constexpr (isPrimitive) {
                auto valOpt = er6::ReadValue<T>(current + offsets[numOffsets - 1]);
                return valOpt.value_or(T{});
            } else {
                T wrapper(current);
                wrapper.setName(getCleanTypeName<T>());
                wrapper.load(loc);
                return wrapper;
            }
        }
    }

    operator T() const { return get(); }
};
