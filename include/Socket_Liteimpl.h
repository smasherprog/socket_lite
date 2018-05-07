#pragma once
#include "Internal/ThreadPool.h"

#if defined(WINDOWS) || defined(_WIN32)
#include <WinSock2.h>
#include <Windows.h>
#include <mswsock.h>
#endif
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

namespace SL {
namespace NET {
    class Context;
    namespace INTERNAL {
        class ContextImpl {

            std::atomic<int> PendingIO;
            ThreadCount ThreadCount_;

          public:
            std::vector<std::thread> Threads;
#if WIN32
            ContextImpl(const ThreadCount &t) : ThreadCount_(t), ConnectEx_(nullptr), IOCPHandle(NULL) { PendingIO = 0; }
            WSADATA wsaData;
            LPFN_CONNECTEX ConnectEx_;
            HANDLE IOCPHandle;
#else
            ContextImpl(const ThreadCount &t) : ThreadCount_(t), EventWakeFd(-1), IOCPHandle(-1) { PendingIO = 0; }
            int EventWakeFd;
            int IOCPHandle;
#endif

            int IncrementPendingIO() { return PendingIO.fetch_add(1, std::memory_order_acquire) + 1; }
            int DecrementPendingIO() { return PendingIO.fetch_sub(1, std::memory_order_acquire) - 1; }
            int getPendingIO() const { return PendingIO.load(std::memory_order_relaxed); }
            int getThreadCount() const { return ThreadCount_.value; }
        };
    } // namespace INTERNAL
} // namespace NET
} // namespace SL
