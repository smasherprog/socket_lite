#pragma once
#include "Socket_Lite.h"
#include <functional>
#include <memory>
namespace SL {
namespace NET {

    enum IO_OPERATION { IoConnect, IoAccept, IoRead, IoWrite };

    struct Socket_IO_Context {
        Bytes_Transfered transfered_bytes = 0;
        Bytes_Transfered bufferlen = 0;
        unsigned char *buffer = nullptr;
        std::function<void(Bytes_Transfered)> completionhandler;
        std::shared_ptr<ISocket> Socket_;
    };
    template <typename T> struct Node {
        T *next = nullptr;
        T *prev = nullptr;
    };
} // namespace NET
} // namespace SL