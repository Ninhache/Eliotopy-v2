#pragma once
#include <string>
#include <typeinfo>

inline thread_local void* g_currentCopyTarget = nullptr;

template<typename T>
static std::string getCleanTypeName() {
    std::string name = typeid(T).name();
    size_t pos;
    while ((pos = name.find("class ")) != std::string::npos) name.erase(pos, 6);
    while ((pos = name.find("struct ")) != std::string::npos) name.erase(pos, 7);
    return name;
}
