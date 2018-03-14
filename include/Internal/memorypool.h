#pragma once
#include "Structures.h"
#include "spinlock.h"
#include <atomic>
#include <thread>
namespace SL {
namespace NET {
    template <class T> class MemoryPool {

        std::vector<T *> RW_ContextBuffer;
        spinlock RW_ContextBufferLock;

      public:
        ~MemoryPool()
        {
            std::lock_guard<spinlock> lock(RW_ContextBufferLock);
            for (auto a : RW_ContextBuffer) {
                delete a;
            }
        }
        T *newObject()
        {
            if (RW_ContextBuffer.empty())
                return new T();
            std::lock_guard<spinlock> lock(RW_ContextBufferLock);
            if (RW_ContextBuffer.empty())
                return new T();
            auto p = RW_ContextBuffer.back();
            RW_ContextBuffer.pop_back();
            return p;
        }
        void deleteObject(T *obj)
        {
            obj->clear();
            std::lock_guard<spinlock> lock(RW_ContextBufferLock);
            RW_ContextBuffer.push_back(obj);
        }
    };
} // namespace NET
} // namespace SL