#pragma once

#include "Internal/spinlock.h"
#include "Socket_Lite.h"
#include <algorithm>
#include <assert.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#if defined(WINDOWS) || defined(_WIN32)
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <mswsock.h>
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#ifndef _WIN32
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#endif

using namespace std::chrono_literals;
namespace SL {
namespace NET {

    StatusCode TranslateError(int *errcode = nullptr);
    enum IO_OPERATION { IoRead, IoWrite, IoConnect, IoAccept, IoNone };

    class RW_Context {
      public:
#ifdef _WIN32
        WSAOVERLAPPED Overlapped = {0};
#endif
      private:
        std::function<void(StatusCode, size_t)> completionhandler;
        std::atomic<int> Completion;

      public:
        IO_OPERATION IOOperation = IO_OPERATION::IoNone; 
        int transfered_bytes = 0;
        int bufferlen = 0;
        unsigned char *buffer = nullptr;
        RW_Context() { clear(); }
        RW_Context(const RW_Context &) { clear(); }
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
        void clear()
        {
            transfered_bytes = bufferlen = 0;
            buffer = nullptr;
            Completion = 0;
            completionhandler = nullptr;
        }
    };
    void completeio(RW_Context &context, Context &iodata, StatusCode code, size_t bytes);
    void continue_io(bool success, RW_Context &context, Context &iodata, const SocketHandle &handle);
    void continue_connect(bool success, RW_Context &context, Context &iodata, const SocketHandle &handle); 


    class NetworkConfig : public INetworkConfig, public std::enable_shared_from_this<NetworkConfig> {
        std::shared_ptr<Context> Context_;

      public:
        NetworkConfig(std::shared_ptr<Context> &&l) : Context_(std::move(l)) {}
        virtual std::shared_ptr<INetworkConfig> AddListener(PlatformSocket &&, std::function<void(Socket)> &&) override;
        virtual std::shared_ptr<Context> run() override;
    }; 

} // namespace NET
} // namespace SL
