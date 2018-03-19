#pragma once

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#endif
#include "Socket_Lite.h"

#include <atomic>
#include <functional>
#include <mutex>

namespace SL {
namespace NET {

    StatusCode SOCKET_LITE_EXTERN TranslateError(int *errcode = nullptr);
    enum IO_OPERATION { IoNone, IoInitConnect, IoConnect, IoStartAccept, IoAccept, IoRead, IoWrite };
    class Socket;
    class Context;
    struct Win_IO_Context {
#ifdef _WIN32
        WSAOVERLAPPED Overlapped = {0};
#endif
        IO_OPERATION IOOperation = IO_OPERATION::IoNone;
    };

    struct Win_IO_Connect_Context : Win_IO_Context {
        SL::NET::sockaddr address;
        std::function<void(StatusCode)> completionhandler;
        Socket *Socket_ = nullptr;
        Context *Context_ = nullptr;
    };

    struct Win_IO_Accept_Context : Win_IO_Context {
#ifdef _WIN32
        char Buffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
#endif
        AddressFamily Family = AddressFamily::IPV4;
        Context *Context_ = nullptr;
        std::shared_ptr<Socket> Socket_;
        PlatformSocket ListenSocket = INVALID_SOCKET;
        std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> completionhandler;
    };
    struct RW_CompletionHandler {
        RW_CompletionHandler() { Completed = 1; }
        std::function<void(StatusCode, size_t)> completionhandler;
        std::atomic<int> Completed;
        void handle(StatusCode code, size_t bytes, bool lockneeded)
        {
            if (lockneeded) {
                if (Completed.fetch_sub(1, std::memory_order_relaxed) == 1) {
                    completionhandler(code, bytes);
                }
            }
            else {
                completionhandler(code, bytes);
            }
        }
    };
    struct Win_IO_RW_Context : Win_IO_Context {
        size_t transfered_bytes = 0;
        size_t bufferlen = 0;
        Context *Context_ = nullptr;
        Socket *Socket_ = nullptr;
        unsigned char *buffer = nullptr;
        std::shared_ptr<RW_CompletionHandler> completionhandler;
    };
} // namespace NET
} // namespace SL
