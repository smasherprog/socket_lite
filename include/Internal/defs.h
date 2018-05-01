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
          PlatformSocket Socket_;
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
            Socket_.close();
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
        PlatformSocket Socket_;
        PlatformSocket ListenSocket;
        std::function<void(StatusCode, Socket)> completionhandler;
        void reset()
        {
            Win_IO_Context::reset();
#ifndef _WIN32
            Listener_ = nullptr;
#endif
            Family = AddressFamily::IPV4;
            Socket_.close();
            ListenSocket.close();
            completionhandler = nullptr;
        }
    };

    void continue_io(bool success, INTERNAL::Win_IO_RW_Context *context, std::atomic<int> &pendingio
#if !WIN32
                     ,
                     int iocphandle, int &stacklevel,
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
     
    struct sockaddr {
        unsigned char SocketImpl[65] = {0};
        int SocketImplLen = 0;
        std::string Host;
        unsigned short Port = 0;
        AddressFamily Family = AddressFamily::IPV4;

        sockaddr() {}
        sockaddr(unsigned char *buffer, int len, const char *host, unsigned short port, AddressFamily family);
        sockaddr(const sockaddr &addr);
    };

} // namespace NET
} // namespace SL
