#pragma once
#include <atomic>

namespace SL {
namespace NET {
    struct spinlock {
        std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
        void lock()
        {
            while (lock_.test_and_set(std::memory_order_acquire))
                ;
        }
        void unlock() { lock_.clear(std::memory_order_release); }
    };

} // namespace NET
} // namespace SL

