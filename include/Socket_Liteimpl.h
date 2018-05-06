#pragma once

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
            ContextImpl *Context_;
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
    } // namespace INTERNAL
} // namespace NET
} // namespace SL
