#pragma once
#include <source_location>
#include "Memory.h"
#include "Il2CppObject.h"

class ScriptableObject : public Il2CppObject {
public:
    using Il2CppObject::load;

    ScriptableObject(uintptr_t addr, const std::source_location& loc = std::source_location::current())
            : Il2CppObject(addr, loc) {}
};
