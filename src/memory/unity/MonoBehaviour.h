#pragma once
#include <source_location>
#include <string>
#include "Memory.h"
#include "Il2CppObject.h"
#include "NativeComponent.h"

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
