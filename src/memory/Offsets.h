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

enum ManagedMapOffsets : size_t {
    dictEntriesArray = 0x18,
    arrayLength      = 0x18,
    arrayData        = 0x20,
    entrySize        = 0x18,
    entryHashCode    = 0x00,
    entryKey         = 0x08,
    entryValue       = 0x10
};

enum ManagedStringOffsets : size_t {
    stringLength    = 0x10,
    firstChar = 0x14
};