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
        C Handlers;

      public:
        MemoryPool() {}
        ~MemoryPool()
        {
            std::lock_guard<spinlock> lock(RW_ContextBufferLock);
            for (auto a : RW_ContextBuffer) {
                Handlers.deleteObj(a);
            }
        }
        T newObject()
        {
            if (RW_ContextBuffer.empty())
                return Handlers.newObj();
            std::lock_guard<spinlock> lock(RW_ContextBufferLock);
            if (RW_ContextBuffer.empty())
                return Handlers.newObj();
            auto p = RW_ContextBuffer.back();
            RW_ContextBuffer.pop_back();
            return p;
        }
        void deleteObject(T obj)
        {
            Handlers.clearObj(obj);
            std::lock_guard<spinlock> lock(RW_ContextBufferLock);
            RW_ContextBuffer.push_back(obj);
        }
    };
} // namespace NET
} // namespace SL