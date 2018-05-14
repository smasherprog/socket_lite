#pragma once

#include "Socket_Lite.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <algorithm>
#include <chrono>

#if defined(WINDOWS) || defined(_WIN32)
#include <WinSock2.h>
#include <Windows.h>
#include <mswsock.h>
#include <Ws2tcpip.h>
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
    class IOData;
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
        size_t transfered_bytes = 0;
        size_t bufferlen = 0;
        PlatformSocket &Socket_;
        IOData &Context_;
        unsigned char *buffer = nullptr;
        Win_IO_Context(PlatformSocket &s, IOData &c) : Socket_(s), Context_(c) { Completion = 0; }
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
    };

    void continue_io(bool success, Win_IO_Context *context);
    void continue_connect(bool success, Win_IO_Context *context);
    class IOData {

        std::atomic<int> PendingIO;
        bool KeepGoing_;
        std::thread Thread;
#if WIN32
        HANDLE IOCPHandle;
        WSADATA wsaData;
#else
        int EventWakeFd;
        int IOCPHandle;
#endif
      public:
#if WIN32
        LPFN_CONNECTEX ConnectEx_;
        IOData()
        {
            PendingIO = 0;
            KeepGoing_ = true;
            if (WSAStartup(0x202, &wsaData) != 0) {
                abort();
            }
            IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);

            auto handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
            GUID guid = WSAID_CONNECTEX;
            DWORD bytes = 0;
            WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);
            closesocket(handle);
            if (IOCPHandle == NULL || ConnectEx_ == nullptr) {
                abort();
            }
            Thread = std::thread([&] {
                while (true) {
                    DWORD numberofbytestransfered = 0;
                    Win_IO_Context *overlapped = nullptr;
                    void *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(IOCPHandle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;
                    if (overlapped == NULL && !KeepGoing_ && getPendingIO() <= 0) {
                        return;
                    }
                    switch (overlapped->IOOperation) {

                    case IO_OPERATION::IoConnect:
                        continue_connect(bSuccess, overlapped);
                        break;
                    case IO_OPERATION::IoRead:
                    case IO_OPERATION::IoWrite:
                        overlapped->transfered_bytes += numberofbytestransfered;
                        if (numberofbytestransfered == 0 && overlapped->bufferlen != 0 && bSuccess) {
                            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                        }
                        continue_io(bSuccess, overlapped);
                        break;
                    default:
                        break;
                    }
                    if (DecrementPendingIO() <= 0 && !KeepGoing_) {
                        return;
                    }
                }
            });
        }
        ~IOData()
        {
            while (getPendingIO() > 0) {
                std::this_thread::sleep_for(10ms);
            }
            wakeup();
            CloseHandle(IOCPHandle);
            WSACleanup();
            Thread.join();
        }
        HANDLE getIOHandle() const { return IOCPHandle; }
        void wakeup() { PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL); }
#else
 
        IOData()
        {
            PendingIO = 0;
            KeepGoing_ = true;
            IOCPHandle = epoll_create1(0);
            EventWakeFd = eventfd(0, EFD_NONBLOCK);
            if (IOCPHandle == -1 || EventWakeFd == -1) {
                abort();
            }
            epoll_event ev = {0};
            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = EventWakeFd;
            if (epoll_ctl(IOCPHandle, EPOLL_CTL_ADD, EventWakeFd, &ev) == 1) {
                abort();
            }
            Thread = std::thread([&] {
                while (true) {
                    epoll_event epollevents[10] ; 
                    while (true) {
                        auto count = epoll_wait(IOCPHandle, epollevents, 10, -1);

                        for (auto i = 0; i < count; i++) {
                            if (epollevents[i].data.fd != EventWakeFd) {
                                auto ctx = static_cast<Win_IO_Context *>(epollevents[i].data.ptr);

                                switch (ctx->IOOperation) {
                                case IO_OPERATION::IoConnect:
                                    continue_connect(true, ctx);
                                    break;
                                case IO_OPERATION::IoRead:
                                case IO_OPERATION::IoWrite:
                                    continue_io(true, ctx);
                                    break;
                                default:
                                    break;
                                }
                                if (DecrementPendingIO() <= 0 && !KeepGoing_) {
                                    return;
                                }
                            }
                        }
                        if (getPendingIO() <= 0 && !KeepGoing_) {
                            return;
                        }
                    }
                }
            });
        }
        ~IOData()
        {
            while (getPendingIO() > 0) {
                std::this_thread::sleep_for(10ms);
                wakeup();
            }
            wakeup();
            Thread.join(); 
            if (EventWakeFd != -1) {
                ::close(EventWakeFd);
            }
            ::close(IOCPHandle);
        }
        int getIOHandle() const { return IOCPHandle; }
        void wakeup() { eventfd_write(EventWakeFd, 1); }
#endif

        int IncrementPendingIO() { return PendingIO.fetch_add(1, std::memory_order_acquire) + 1; }
        int DecrementPendingIO() { return PendingIO.fetch_sub(1, std::memory_order_acquire) - 1; }
        int getPendingIO() const { return PendingIO.load(std::memory_order_relaxed); }
        void stop() { KeepGoing_ = false; }
    };
    class ContextImpl {
        const ThreadCount ThreadCount_;
        std::unique_ptr<IOData[]> ThreadData;
        int LastThreadIndex;

      public:
        ContextImpl(ThreadCount t) : ThreadCount_(t)
        {
            ThreadData = std::make_unique<IOData[]>(ThreadCount_.value);
            LastThreadIndex = 0;
        }
        ~ContextImpl()
        {
            for (decltype(ThreadCount_.value) i = 0; i < ThreadCount_.value; i++) {
                ThreadData[i].stop();
            } 
        }
        IOData &getIOData() { return ThreadData[LastThreadIndex++ % ThreadCount_.value]; }
    };
} // namespace NET
} // namespace SL
