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

#if _WIN32
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
    enum IO_OPERATION : unsigned int { IoRead, IoWrite, IoConnect, IoAccept };
    class RW_Context {
      public:
#ifdef _WIN32
        WSAOVERLAPPED Overlapped = {0};
#endif
      private:
        std::atomic<Handler> completionhandler;
        int remaining_bytes;
        void *UserData;

      public:
        unsigned char *buffer = nullptr;
        RW_Context() { clear(); }
        RW_Context(const RW_Context &) { clear(); }
        void setUserData(void *userdata) { UserData = userdata; }
        void *getUserData() const { return UserData; }
        void setCompletionHandler(Handler c)
        {
#if _WIN32
            Overlapped = {0};
#endif
            completionhandler.store(c, std::memory_order_relaxed);
        }
        Handler getCompletionHandler()
        {
            auto h = completionhandler.load(std::memory_order_relaxed);
            if (h) {
                while (!completionhandler.compare_exchange_weak(h, nullptr, std::memory_order_release, std::memory_order_relaxed))
                    ; // empty on purpose
            }
            return h;
        }
        void setRemainingBytes(int remainingbytes)
        {
            auto p = (remainingbytes & ~0xC0000000);
            auto p1 = (remaining_bytes & 0xC0000000);

            remaining_bytes = p | p1;
        }
        int getRemainingBytes() const { return remaining_bytes & ~0xC0000000; }
        void setEvent(IO_OPERATION op) { remaining_bytes = getRemainingBytes() | (op << 30); }
        IO_OPERATION getEvent() const { return static_cast<IO_OPERATION>((remaining_bytes >> 30) & 0x00000003); }

        void clear()
        {
            remaining_bytes = 0;
            buffer = nullptr;
            UserData = nullptr;
            completionhandler = nullptr;
        }
    };
    void completeio(RW_Context &context, Context &iodata, StatusCode code);
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
