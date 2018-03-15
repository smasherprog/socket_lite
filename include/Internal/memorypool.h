#pragma once
#include "Structures.h"
#include "spinlock.h"
#include <atomic>
#include <functional>
#include <thread>

namespace SL {
namespace NET {
    template <class T, class C> class MemoryPool {

        std::vector<T> RW_ContextBuffer;
        spinlock RW_ContextBufferLock;
        int ThreadCount = 1;
        C Handlers;
        int skippedalloc = 0;
        int totalalloc = 0;
        int reclaimed = 0;

      public:
        MemoryPool(int threadcount) : ThreadCount(threadcount) {}
        ~MemoryPool()
        {
            if (ThreadCount > 1) {
                std::lock_guard<spinlock> lock(RW_ContextBufferLock);
                for (auto a : RW_ContextBuffer) {
                    Handlers.deleteObj(a);
                }
            }
            else {
                for (auto a : RW_ContextBuffer) {
                    Handlers.deleteObj(a);
                }
            }
        }
        T newObject()
        {
            totalalloc += 1;
            if (ThreadCount > 1) {
                if (RW_ContextBuffer.empty())
                    return Handlers.newObj();
                std::lock_guard<spinlock> lock(RW_ContextBufferLock);
                if (RW_ContextBuffer.empty())
                    return Handlers.newObj();
                auto p = RW_ContextBuffer.back();
                RW_ContextBuffer.pop_back();
                skippedalloc += 1;
                return p;
            }
            else {
                if (RW_ContextBuffer.empty())
                    return Handlers.newObj();
                auto p = RW_ContextBuffer.back();
                RW_ContextBuffer.pop_back();
                return p;
            }
        }
        void deleteObject(T obj)
        {
            reclaimed += 1;
            Handlers.clearObj(obj);
            if (ThreadCount > 1) {
                std::lock_guard<spinlock> lock(RW_ContextBufferLock);
                RW_ContextBuffer.push_back(obj);
            }
            else {
                RW_ContextBuffer.push_back(obj);
            }
        }
    };
} // namespace NET
} // namespace SL