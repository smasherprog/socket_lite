#pragma once
#include "Structures.h"
#include "spinlock.h"
#include <assert.h>
#include <atomic>
#include <cstddef> // size_t
#include <functional>
#include <thread>

namespace SL {
namespace NET {
    struct MallocatorImpl {

        std::vector<void *> Buffer;
        size_t TotalSize = 0;
        size_t ChunkSize = 0;
        spinlock Lock;
        MallocatorImpl(size_t chunksize, size_t totalchunks)
        {
            TotalSize = totalchunks;
            ChunkSize = chunksize;
            Buffer.resize(totalchunks);
            for (auto &a : Buffer) {
                a = std::malloc(chunksize);
            }
        }
        ~MallocatorImpl()
        {
            for (auto a : Buffer) {
                std::free(a);
            }
        }
        template <class T> T *allocate(std::size_t n)
        {
            if (n > std::size_t(-1) / sizeof(T))
                throw std::bad_alloc();
            auto s = sizeof(T);
            assert(n * sizeof(T) <= ChunkSize);
            T *buffer = nullptr;
            if (Buffer.empty()) {
                buffer = static_cast<T *>(malloc(n * sizeof(T)));
            }
            else {
                std::lock_guard<spinlock> lock(Lock);
                if (!Buffer.empty()) {
                    buffer = static_cast<T *>(Buffer.back());
                    Buffer.pop_back();
                }
            }
            if (buffer == nullptr) {
                buffer = static_cast<T *>(malloc(n * sizeof(T)));
            }
            if constexpr (std::is_default_constructible<T>::value) {
                new (buffer) T();
            }
            return buffer;
        }
        template <class T> void deallocate(T *p) noexcept
        {
            p->~T();
            if (Buffer.size() >= TotalSize) {
                free(p);
            }
            else {
                std::lock_guard<spinlock> lock(Lock);
                Buffer.push_back(p);
            }
        }
    };

    template <class T> struct Mallocator {
        Mallocator(Mallocator<T> &other) : Impl(other.Impl) {}
        template <typename U> Mallocator(const Mallocator<U> &other) : Impl(other.Impl) {}
        Mallocator(MallocatorImpl *impl) : Impl(impl) {}
        MallocatorImpl *Impl;
        typedef T value_type;
        template <typename U> struct rebind {
            typedef Mallocator<U> other;
        };

        [[nodiscard]] T *allocate(std::size_t n) { return Impl->allocate<T>(n); }
        void deallocate(T *p, std::size_t) noexcept { Impl->deallocate<T>(p); }
    };
    template <class T, class U> bool operator==(const Mallocator<T> &, const Mallocator<U> &) { return true; }
    template <class T, class U> bool operator!=(const Mallocator<T> &, const Mallocator<U> &) { return false; }

} // namespace NET
} // namespace SL