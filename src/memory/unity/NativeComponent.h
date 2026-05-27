#pragma once
#include <source_location>
#include "Memory.h"
#include "Offsets.h"
#include "Il2CppObject.h"

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
