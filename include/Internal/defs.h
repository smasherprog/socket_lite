#pragma once

#include "Socket_Lite.h"

#include <atomic>
#include <functional>
#include <mutex>

namespace SL {
namespace NET {

    StatusCode TranslateError(int *errcode = nullptr);

    enum IO_OPERATION { IoConnect, IoRead, IoWrite, IoNone };
    class Win_IO_Context {
      public:
#ifdef _WIN32
        WSAOVERLAPPED Overlapped = {0};
#endif

        std::atomic<int> RefCount;

        IO_OPERATION IOOperation = IO_OPERATION::IoNone;
        Win_IO_Context() { RefCount = 1; }
        int IncrementRef() { return RefCount.fetch_add(1, std::memory_order_acquire) + 1; }
        int DecrementRef() { return RefCount.fetch_sub(1, std::memory_order_acquire) - 1; }
        void reset()
        {
            RefCount = 1;
#ifdef _WIN32
            Overlapped = {0};
#endif
            IOOperation = IO_OPERATION::IoNone;
        }
    };

    class Win_IO_RW_Context : public Win_IO_Context {
        std::function<void(StatusCode, size_t)> completionhandler;
        std::atomic<int> Completion;

      public:
        size_t transfered_bytes;
        size_t bufferlen;
        SocketHandle Socket_;
        INTERNAL::ContextImpl *Context_;
        unsigned char *buffer;
        Win_IO_RW_Context() : Socket_(INVALID_SOCKET) { reset(); }
        void setCompletionHandler(std::function<void(StatusCode, size_t)> &&c)
        {
            completionhandler = std::move(c);
            Completion = 1;
        }
        std::function<void(StatusCode, size_t)> getCompletionHandler()
        {
            if (Completion.fetch_sub(1, std::memory_order_acquire) == 1) {
                return std::move(completionhandler);
            }
            std::function<void(StatusCode, size_t)> t;
            return t;
        }
        void reset()
        {
            Socket_.value = INVALID_SOCKET;
            transfered_bytes = 0;
            bufferlen = 0;
            buffer = nullptr;
            Completion = 1;
            completionhandler = nullptr;
            Win_IO_Context::reset();
        }
    };
    class Win_IO_Connect_Context : public Win_IO_Context {
        std::function<void(StatusCode, Socket &&)> completionhandler;
        std::atomic<int> Completion;

      public:
        PlatformSocket Socket_;
        INTERNAL::ContextImpl *Context_ = nullptr;
        Win_IO_Connect_Context() { Completion = 0; }
        void setCompletionHandler(std::function<void(StatusCode, Socket &&)> &&c)
        {
            completionhandler = std::move(c);
            Completion = 1;
        }
        std::function<void(StatusCode, Socket &&)> getCompletionHandler()
        {
            if (Completion.fetch_sub(1, std::memory_order_acquire) == 1) {
                return std::move(completionhandler);
            }
            std::function<void(StatusCode, Socket &&)> t;
            return t;
        }

        void reset()
        {
            Completion = 1;
            completionhandler = nullptr;
            Win_IO_Context::reset();
            [[maybe_unused]] auto ret = Socket_.close();
        }
    };
    struct Win_IO_Accept_Context : Win_IO_Context {
#ifdef _WIN32
        char Buffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
#else
        Listener *Listener_ = nullptr;
#endif
        AddressFamily Family = AddressFamily::IPV4;
        Context *Context_ = nullptr;
        PlatformSocket Socket_;
        PlatformSocket ListenSocket;
        std::function<void(StatusCode, Socket &&)> completionhandler;
        void reset()
        {
            Win_IO_Context::reset();
#ifndef _WIN32
            Listener_ = nullptr;
#endif
            Family = AddressFamily::IPV4;
            [[maybe_unused]] auto ret = Socket_.close();
            [[maybe_unused]] auto ret1 = ListenSocket.close();
            completionhandler = nullptr;
        }
    };

    void continue_io(bool success, Win_IO_RW_Context *context);

    void continue_connect(bool success,
#if WIN32
                          Win_IO_Connect_Context
#else
                          INTERNAL::Win_IO_RW_Context
#endif
                              *context);

    void CloseSocket(PlatformSocket &handle);

} // namespace NET
} // namespace SL
