#pragma once
#include <cstddef>

enum UnityOffsets : size_t {
    cachedPtr = 0x10,
    gcHandle = 0x18,
    gcHandleTarget = 0x0,
    listArrayPtr = 0x10,
    listSize = 0x18,
    listItemsStart = 0x20,
    pointerSize = 0x8
};