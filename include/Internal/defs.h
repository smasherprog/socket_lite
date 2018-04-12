#pragma once

#include "Socket_Lite.h"

#include <atomic>
#include <functional>
#include <mutex>

namespace SL {
namespace NET {

    StatusCode TranslateError(int *errcode = nullptr);

    class Win_IO_Connect_Context : public INTERNAL::Win_IO_Context {
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
    struct Win_IO_Accept_Context : INTERNAL::Win_IO_Context {
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

    namespace INTERNAL {
        PlatformSocket Socket(AddressFamily family);
    }
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
        INTERNAL::Win_IO_RW_Context *getReadContext() const { return &socket.ReadContext; }
        INTERNAL::Win_IO_RW_Context *getWriteContext() const { return &socket.WriteContext; }
        std::atomic<int> &getPendingIO() const { return context.PendingIO; }

#if WIN32
        LPFN_CONNECTEX ConnectEx() const { return context.ConnectEx_; }
        HANDLE getIOCPHandle() const { return context.IOCPHandle; }
#else
        int getIOCPHandle() const { return context.IOCPHandle; }
#endif
    };

    void continue_io(bool success, INTERNAL::Win_IO_RW_Context *context, std::atomic<int> &pendingio
#if !WIN32    
    ,int iocphandle
    ,int& stacklevel,
#endif
    );
    
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
