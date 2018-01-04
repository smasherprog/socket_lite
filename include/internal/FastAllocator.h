#pragma once
#include <stdlib.h>

namespace SL {
namespace NET {
    class FastAllocator {
        unsigned char *buffer_;
        size_t size_;
        size_t capacity_;

      public:
        FastAllocator(FastAllocator &&) = delete;
        FastAllocator &operator=(FastAllocator &&) = delete;
        FastAllocator() : buffer_(nullptr), size_(0), capacity_(0) { capacity(1024); }
        ~FastAllocator()
        {
            if (buffer_) {
                free(buffer_);
            }
        }
        unsigned char *data() const { return buffer_; }
        size_t size() const { return size_; }
        void size(size_t s) { size_ = s; }
        size_t capacity() const { return capacity_; }
        void grow() { capacity(capacity_ * 2); }
        void capacity(size_t c)
        {
            capacity_ = c;
            buffer_ = (unsigned char *)realloc(buffer_, c);
            size_ = size_ > capacity_ ? capacity_ : size_; // cap size
        }
    };
} // namespace NET
} // namespace SL