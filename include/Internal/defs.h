#pragma once

#include "Socket_Lite.h"
#include <atomic>
#include <functional>
#include <mutex>

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

    StatusCode TranslateError(int *errcode = nullptr);

    enum IO_OPERATION { IoConnect, IoRead, IoWrite, IoNone };
    class Win_IO_Context {

      public:
#ifdef _WIN32
        WSAOVERLAPPED Overlapped = {0};
#endif
        IO_OPERATION IOOperation = IO_OPERATION::IoNone;

      protected:
        std::function<void(StatusCode, size_t)> completionhandler;
        std::atomic<int> Completion;

      public:
        size_t transfered_bytes;
        size_t bufferlen;
        PlatformSocket *Socket_;
        ContextImpl *Context_;
        unsigned char *buffer;
        Win_IO_Context() { reset(); }
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
            Socket_ = nullptr;
            transfered_bytes = 0;
            bufferlen = 0;
            buffer = nullptr;
            Completion = 1;
            completionhandler = nullptr;
#ifdef _WIN32
            Overlapped = {0};
#endif
            IOOperation = IO_OPERATION::IoNone;
        }
    };

    void continue_io(bool success, Win_IO_Context *context);
    void continue_connect(bool success, Win_IO_Context *context);

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

} // namespace NET
} // namespace SL
