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
namespace SL
{
namespace NET
{

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
    void reset() {
#ifdef _WIN32
        Overlapped = {0};
#endif
        IOOperation = IO_OPERATION::IoNone;
    }
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
#else
    Listener *Listener_ = nullptr;
#endif
    AddressFamily Family = AddressFamily::IPV4;
    Context *Context_ = nullptr;
    std::shared_ptr<Socket> Socket_;
    PlatformSocket ListenSocket = INVALID_SOCKET;
    std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> completionhandler;
};

#ifdef _WIN32
struct RW_CompletionHandler {
    RW_CompletionHandler() {
        Completed = 1;
    }
    std::function<void(StatusCode, size_t)> completionhandler;
    std::atomic<int> Completed;
    void handle(StatusCode code, size_t bytes, bool lockneeded) {
        if (lockneeded) {
            if (Completed.fetch_sub(1, std::memory_order_relaxed) == 1) {
                completionhandler(code, bytes);
            }
        } else {
            completionhandler(code, bytes);
        }
    }
};
#else
struct RW_CompletionHandler {
    std::function<void(StatusCode, size_t)> completionhandler;
    void handle(StatusCode code, size_t bytes) {
        completionhandler(code, bytes);
    }
};
#endif

class Win_IO_RW_Context : public Win_IO_Context
{
    std::shared_ptr<RW_CompletionHandler> completionhandler;

public:
    size_t transfered_bytes = 0;
    size_t bufferlen = 0;
    Context *Context_ = nullptr;
    Socket *Socket_ = nullptr;
    unsigned char *buffer = nullptr;
#ifdef _WIN32
    void setCompletionHandler(std::shared_ptr<RW_CompletionHandler> &c) {
        completionhandler = c;
    }
    std::shared_ptr<RW_CompletionHandler> getCompletionHandler() {
        return completionhandler;
    }
#else
    void setCompletionHandler(std::shared_ptr<RW_CompletionHandler> &c) {
        std::atomic_store(&completionhandler, c);
    }
    std::shared_ptr<RW_CompletionHandler> getCompletionHandler() {
        return std::atomic_exchange(&completionhandler, std::shared_ptr<RW_CompletionHandler>());
    }
#endif
    void reset() {
        transfered_bytes = 0;
        bufferlen = 0;
        buffer = nullptr;
#ifdef _WIN32
        completionhandler = nullptr;
#else
        std::atomic_store(&completionhandler, std::shared_ptr<RW_CompletionHandler>());
#endif

        Win_IO_Context::reset();
    }
};
} // namespace NET
} // namespace SL
