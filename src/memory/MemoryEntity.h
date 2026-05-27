#pragma once
#include <cstdint>
#include <string>
#include <source_location>
#include "Resolve6.hpp"
#include "AppLogger.h"

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
