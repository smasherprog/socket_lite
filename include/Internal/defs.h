#pragma once

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
    enum IO_OPERATION { IoConnect, IoAccept, IoRead, IoWrite, IoNone };
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

    class Win_IO_Connect_Context : public Win_IO_Context {
        std::function<void(StatusCode)> completionhandler;
        std::atomic<int> Completion;

      public:
        PlatformSocket Socket_ = INVALID_SOCKET;
        Context *Context_ = nullptr;
        Win_IO_Connect_Context() { Completion = 0; }
        void setCompletionHandler(std::function<void(StatusCode)> &&c)
        {
            completionhandler = std::move(c);
            Completion = 1;
        }
        std::function<void(StatusCode)> getCompletionHandler()
        {
            if (Completion.fetch_sub(1, std::memory_order_acquire) == 1) {
                return std::move(completionhandler);
            }
            std::function<void(StatusCode)> t;
            return t;
        }

        void reset()
        {
            Socket_ = INVALID_SOCKET;
            Completion = 1;
            completionhandler = nullptr;
            Win_IO_Context::reset();
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
        PlatformSocket Socket_ = INVALID_SOCKET;
        PlatformSocket ListenSocket = INVALID_SOCKET;
        std::function<void(StatusCode, Socket)> completionhandler;
        void reset()
        {
            Win_IO_Context::reset();
#ifndef _WIN32
            Listener_ = nullptr;
#endif
            Family = AddressFamily::IPV4;
            Socket_ = ListenSocket = INVALID_SOCKET;
            completionhandler = nullptr;
        }
    };

    class Win_IO_RW_Context : public Win_IO_Context {
        std::function<void(StatusCode, size_t)> completionhandler;
        std::atomic<int> Completion;

      public:
        size_t transfered_bytes = 0;
        size_t bufferlen = 0;
        PlatformSocket Socket_ = INVALID_SOCKET;
        Context *Context_ = nullptr;
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
            Socket_ = INVALID_SOCKET;
            transfered_bytes = 0;
            bufferlen = 0;
            buffer = nullptr;
            Completion = 1;
            completionhandler = nullptr;
            Win_IO_Context::reset();
        }
    };

    namespace INTERNAL {
        PlatformSocket Socket(AddressFamily family);
    };
    class SocketGetter {
        Socket &socket;
        Context &context;

      public:
        SocketGetter(Socket &s) : socket(s), context(s.context) {}
        StatusCode bind(sockaddr addr);
        StatusCode listen(int backlog);
        PlatformSocket getSocket() const { return socket.handle; }
        PlatformSocket setSocket(PlatformSocket s)
        {
            socket.handle = s;
            return s;
        }
        Context *getContext() const { return &context; }
        std::atomic<int> &getPendingIO() const { return context.PendingIO; }

#if WIN32
        LPFN_CONNECTEX ConnectEx() const { return context.ConnectEx_; }
        HANDLE getIOCPHandle() const { return context.IOCPHandle; }
#else
        int getIOCPHandle() const { return context.IOCPHandle; }
#endif
    };

    void continue_io(bool success, Win_IO_RW_Context *context, std::atomic<int> &pendingio);
    void continue_connect(bool success, Win_IO_Connect_Context *context);
    void handle_accept(bool success, Win_IO_Accept_Context *context, Socket &&s);

} // namespace NET
} // namespace SL
