#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "../../mem/memory_read.hpp"
#include "gom_bucket.hpp"
#include "gom_list_node.hpp"
#include "gom_offsets.hpp"
#include "gom_pool.hpp"
#include "gom_walker_types.hpp"
#include "../core/offsets.hpp"

namespace er6
{

inline bool GetManagedFromNative(const IMemoryAccessor& mem, std::uintptr_t nativeObject, const GomOffsets& gomOff, const Offsets& off, std::uintptr_t& outManaged)
{
    outManaged = 0;
    if (!nativeObject)
    {
        return false;
    }
    
    // Native GameObject -> GCHandle
    std::uintptr_t gchandle = 0;
    if (!ReadPtr(mem, nativeObject + gomOff.game_object.managed, gchandle) || !gchandle)
    {
        return false;
    }
    
    // GCHandle -> Managed Object
    return ReadPtr(mem, gchandle + off.gchandle_to_managed, outManaged) && outManaged != 0;
}

inline bool GetManagedFromComponent(const IMemoryAccessor& mem, std::uintptr_t nativeComponent, const GomOffsets& gomOff, const Offsets& off, std::uintptr_t& outManaged)
{
    outManaged = 0;
    if (!nativeComponent)
    {
        return false;
    }
    
    // Native Component -> GCHandle
    std::uintptr_t gchandle = 0;
    if (!ReadPtr(mem, nativeComponent + gomOff.component.managed, gchandle) || !gchandle)
    {
        return false;
    }
    
    // GCHandle -> Managed Object
    return ReadPtr(mem, gchandle + off.gchandle_to_managed, outManaged) && outManaged != 0;
}

class GomWalker
{
public:
    GomWalker(const IMemoryAccessor& mem, std::uintptr_t gomGlobalSlot, const GomOffsets& gomOff, const Offsets& off)
        : mem_(mem), gomGlobalSlot_(gomGlobalSlot), gomOff_(gomOff), off_(off)
    {
    }

    bool EnumerateGameObjects(std::vector<GameObjectEntry>& out) const
    {
        out.clear();

        std::uintptr_t managerAddress = 0;
        if (!GetGomManagerFromGlobalSlot(mem_, gomGlobalSlot_, managerAddress))
        {
            return false;
        }

        std::unordered_set<std::uintptr_t> visitedNodes;

        // 遍历 bucket 链表
        std::vector<std::uintptr_t> buckets;
        if (GetAllLinkedBucketsFromManager(mem_, managerAddress, gomOff_, buckets) && !buckets.empty())
        {
            for (std::uintptr_t bucketPtr : buckets)
            {
                std::uintptr_t listHead = 0;
                if (!GetBucketListHead(mem_, bucketPtr, gomOff_, listHead))
                {
                    continue;
                }

                std::uintptr_t node = 0;
                if (!GetListNodeFirst(mem_, listHead, gomOff_, node))
                {
                    continue;
                }

                for (; node;)
                {
                    if (visitedNodes.find(node) != visitedNodes.end())
                    {
                        break;
                    }
                    visitedNodes.insert(node);

                    std::uintptr_t nativeObject = 0;
                    if (!GetListNodeNative(mem_, node, gomOff_, nativeObject))
                    {
                        break;
                    }

                    std::uintptr_t managedObject = 0;
                    if (nativeObject)
                    {
                        GetManagedFromNative(mem_, nativeObject, gomOff_, off_, managedObject);
                    }

                    if (nativeObject || managedObject)
                    {
                        GameObjectEntry e;
                        e.node = node;
                        e.nativeObject = nativeObject;
                        e.managedObject = managedObject;
                        out.push_back(e);
                    }

                    std::uintptr_t next = 0;
                    if (!GetListNodeNext(mem_, node, gomOff_, next) || !next || next == listHead)
                    {
                        break;
                    }

                    node = next;
                }
            }
        }

        // 遍历 +0x18 链表
        std::uintptr_t localListHead = 0;
        if (GetGomLocalGameObjectListHead(mem_, managerAddress, gomOff_, localListHead) && localListHead)
        {
            std::uintptr_t node = localListHead;

            for (; node;)
            {
                if (visitedNodes.find(node) != visitedNodes.end())
                {
                    break;
                }
                visitedNodes.insert(node);

                std::uintptr_t nativeObject = 0;
                if (!GetListNodeNative(mem_, node, gomOff_, nativeObject))
                {
                    break;
                }

                std::uintptr_t managedObject = 0;
                if (nativeObject)
                {
                    GetManagedFromNative(mem_, nativeObject, gomOff_, off_, managedObject);
                }

                if (nativeObject || managedObject)
                {
                    GameObjectEntry e;
                    e.node = node;
                    e.nativeObject = nativeObject;
                    e.managedObject = managedObject;
                    out.push_back(e);
                }

                std::uintptr_t next = 0;
                if (!GetListNodeNext(mem_, node, gomOff_, next) || !next || next == localListHead)
                {
                    break;
                }

                node = next;
            }
        }

        return !out.empty();
    }

    bool EnumerateComponents(std::vector<ComponentEntry>& out) const
    {
        out.clear();

        std::vector<GameObjectEntry> gameObjects;
        if (!EnumerateGameObjects(gameObjects) || gameObjects.empty())
        {
            return false;
        }

        const int kMaxComponentsPerObject = 1024;

        for (const auto& go : gameObjects)
        {
            if (!go.nativeObject)
            {
                continue;
            }

            std::uintptr_t pool = 0;
            if (!GetComponentPool(mem_, go.nativeObject, gomOff_, pool) || !pool)
            {
                continue;
            }

            std::int32_t count = 0;
            if (!GetComponentCount(mem_, go.nativeObject, gomOff_, count))
            {
                continue;
            }

            if (count <= 0 || count > kMaxComponentsPerObject)
            {
                continue;
            }

            for (int i = 0; i < count; ++i)
            {
                std::uintptr_t nativeComponent = 0;
                if (!GetComponentSlotNative(mem_, pool, gomOff_, i, nativeComponent))
                {
                    continue;
                }

                std::uintptr_t managedComponent = 0;
                GetManagedFromComponent(mem_, nativeComponent, gomOff_, off_, managedComponent);

                ComponentEntry ce;
                ce.nativeComponent = nativeComponent;
                ce.managedComponent = managedComponent;
                out.push_back(ce);
            }
        }

        return true;
    }

private:
    const IMemoryAccessor& mem_;
    std::uintptr_t gomGlobalSlot_ = 0;
    const GomOffsets& gomOff_;
    const Offsets& off_;
};

} // namespace er6
