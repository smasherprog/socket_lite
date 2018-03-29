#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#endif
#include "Socket_Lite.h"

#include <atomic>
#include <functional>
#include <mutex>
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif
namespace SL {
namespace NET {

    StatusCode SOCKET_LITE_EXTERN TranslateError(int *errcode = nullptr);
    enum IO_OPERATION { IoNone, IoInitConnect, IoConnect, IoStartAccept, IoAccept, IoRead, IoWrite };
    class Socket;
    class Context;
    class Listener;
    struct Win_IO_Context {
#ifdef _WIN32
        WSAOVERLAPPED Overlapped = {0};
#endif
        IO_OPERATION IOOperation = IO_OPERATION::IoNone;
        void reset()
        {
#ifdef _WIN32
            Overlapped = {0};
#endif
            IOOperation = IO_OPERATION::IoNone;
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
        std::shared_ptr<Socket> Socket_;
        PlatformSocket ListenSocket = INVALID_SOCKET;
        std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> completionhandler;
    };

    class Win_IO_RW_Context : public Win_IO_Context {
        std::function<void(StatusCode, size_t)> completionhandler;
        std::atomic<int> Completion;

      public:
        size_t transfered_bytes = 0;
        size_t bufferlen = 0;
        Context *Context_ = nullptr;
        Socket *Socket_ = nullptr;
        unsigned char *buffer = nullptr;
        Win_IO_RW_Context() { Completion = 0; }
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
            transfered_bytes = 0;
            bufferlen = 0;
            buffer = nullptr;
            Completion = 1;
            completionhandler = nullptr;
            Win_IO_Context::reset();
        }
    };
} // namespace NET
} // namespace SL
