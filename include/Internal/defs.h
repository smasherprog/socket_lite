#pragma once

#include "Internal/spinlock.h"
#include "Socket_Lite.h"
#include <algorithm>
#include <assert.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
#include <csignal>

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
        unsigned char *buffer = nullptr;
        Win_IO_Context() { clear(); }
        Win_IO_Context(const Win_IO_Context &) { clear(); }
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

    void completeio(Win_IO_Context &context, ContextImpl &iodata, StatusCode code, size_t bytes);
    void continue_io(bool success, Win_IO_Context &context, ContextImpl &iodata, const SocketHandle &handle);
    void continue_connect(bool success, Win_IO_Context &context, ContextImpl &iodata, const SocketHandle &handle);

    class ContextImpl {
        const ThreadCount ThreadCount_;
        std::vector<std::thread> Threads;
        std::vector<Win_IO_Context> ReadContexts, WriteContexts;
        bool KeepGoing_;
        std::atomic<int> PendingIO;
#if _WIN32
        HANDLE IOCPHandle;
        WSADATA wsaData;

      public:
        LPFN_CONNECTEX ConnectEx_;
#else
        int EventWakeFd;
        int EventFd;
        int IOCPHandle;
        std::vector<int> ReadSockets, WriteSockets;
        spinlock ReadSocketLock, WriteSocketLock;

      public:
#endif

        ContextImpl(ThreadCount t) : ThreadCount_(t)
        {
            KeepGoing_ = true;
            PendingIO = 0;
            Threads.reserve(ThreadCount_.value);
            ReadContexts.resize(std::numeric_limits<unsigned short>::max());
            WriteContexts.resize(std::numeric_limits<unsigned short>::max());
            ReadContexts.shrink_to_fit();
            WriteContexts.shrink_to_fit();
#if _WIN32

            IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, t.value);
            if (WSAStartup(0x202, &wsaData) != 0) {
                abort();
            }
            auto handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
            GUID guid = WSAID_CONNECTEX;
            DWORD bytes = 0;
            WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);
            closesocket(handle);
            if (ConnectEx_ == nullptr) {
                abort();
            }
            for (decltype(ThreadCount::value) i = 0; i < ThreadCount_.value; i++) {
                Threads.emplace_back(std::thread([&] {
                  for(;;){
                        DWORD numberofbytestransfered = 0;
                        Win_IO_Context *overlapped = nullptr;
                        void *completionkey = nullptr;

                        auto bSuccess = GetQueuedCompletionStatus(IOCPHandle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                                  (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;
                        if (overlapped == NULL) {
                            wakeup();
                            return;
                        }
                        auto handle = SocketHandle(reinterpret_cast<decltype(SocketHandle::value)>(completionkey));

                        switch (overlapped->IOOperation) {

                        case IO_OPERATION::IoConnect:
                            continue_connect(bSuccess, *overlapped, *this, handle);
                            break;
                        case IO_OPERATION::IoRead:
                        case IO_OPERATION::IoWrite:
                            overlapped->transfered_bytes += numberofbytestransfered;
                            if (numberofbytestransfered == 0 && overlapped->bufferlen != 0 && bSuccess) {
                                bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                            }
                            continue_io(bSuccess, *overlapped, *this, handle);
                            break;
                        default:
                            break;
                        }
                        if (getPendingIO() <= 0 && !KeepGoing_) {
                            wakeup();
                            return;
                        }
                    }
                }));
            }
#else
            std::signal(SIGPIPE, SIG_IGN);
            IOCPHandle = epoll_create1(EPOLL_CLOEXEC);
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

            EventFd = eventfd(0, EFD_NONBLOCK);
            if (EventFd == -1) {
                abort();
            }
            ev = {0};
            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = EventFd;
            if (epoll_ctl(IOCPHandle, EPOLL_CTL_ADD, EventFd, &ev) == 1) {
                abort();
            }

            for (decltype(ThreadCount::value) i = 0; i < ThreadCount_.value; i++) {
                Threads.emplace_back(std::thread([&] {
                    epoll_event epollevents[128];
                    std::vector<int> socketbuffer;
                    for (;;) {
                        auto count = epoll_wait(IOCPHandle, epollevents, 128, -1);

                        for (auto i = 0; i < count; i++) {
                            if (epollevents[i].data.fd == EventFd) {
                             //   eventfd_t efd = 0;
                               // eventfd_read(EventFd, &efd); 
                            }
                            else if (epollevents[i].data.fd != EventWakeFd) {
                                auto socketclosed = epollevents[i].events & EPOLLERR || epollevents[i].events & EPOLLHUP;

                                SocketHandle handle(epollevents[i].data.fd);

                                if (epollevents[i].events & EPOLLIN) {
                                    auto& rctx = getReadContext(handle);
                                    continue_io(!socketclosed, rctx, *this, handle);
                                }
                                else {
                                    auto& wctx = getWriteContext(handle);
                                    if (wctx.IOOperation == IO_OPERATION::IoConnect) {
                                        continue_connect(!socketclosed, wctx, *this, handle);
                                    }
                                    else if (wctx.IOOperation == IO_OPERATION::IoWrite) {
                                        continue_io(!socketclosed, wctx, *this, handle);
                                    }
                                }
                            }
                        }
                        if (!ReadSockets.empty()) {
                            {
                                std::lock_guard<spinlock> lock(ReadSocketLock);
                                for (auto a : ReadSockets) {
                                    socketbuffer.push_back(a);
                                }
                                ReadSockets.clear();
                            }
                            for (auto a : socketbuffer) {
                                SocketHandle handle(a);
                                auto &rctx = getReadContext(handle);
                                continue_io(true, rctx, *this, handle);
                            }
                            socketbuffer.clear();
                        }

                        if (!WriteSockets.empty()) {
                            {
                                std::lock_guard<spinlock> lock(WriteSocketLock);
                                for (auto a : WriteSockets) {
                                    socketbuffer.push_back(a);
                                }
                                WriteSockets.clear();
                            }
                            for (auto a : socketbuffer) {
                                SocketHandle handle(a);
                                auto& rctx = getWriteContext(handle);
                                continue_io(true, rctx, *this, handle);
                            }
                            socketbuffer.clear();
                        }
                        if (getPendingIO() <= 0 && !KeepGoing_) {
                            wakeup();
                            return;
                        }
                    }
                }));
            }
#endif
        }
        ~ContextImpl()
        {
            stop();
            for (auto& rcts : ReadContexts) {
                completeio(rcts, *this, StatusCode::SC_CLOSED, 0);
            }
            for (auto &rcts : WriteContexts) {

                completeio(rcts, *this, StatusCode::SC_CLOSED, 0);
            }
#ifndef _WIN32
            while (getPendingIO() > 0) {
                std::this_thread::sleep_for(10ms);
                wakeup();
            }
#else
            while (getPendingIO() > 0) {
                std::this_thread::sleep_for(10ms);
            }
#endif

            wakeup();
            for (auto &t : Threads) {
                if (t.joinable()) {
                    t.join();
                }
            }
            Threads.clear(); // force release here
            ReadContexts.clear();
            WriteContexts.clear();
#if _WIN32
            CloseHandle(IOCPHandle);
            WSACleanup();
#else
            ::close(EventFd);
            ::close(EventWakeFd);
            ::close(IOCPHandle);
#endif
        }
#if _WIN32
        HANDLE getIOHandle() const { return IOCPHandle; }
        void wakeup() { PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL); }
#else
        int getIOHandle() const { return IOCPHandle; }
        void wakeup() { eventfd_write(EventWakeFd, 1); }
        void wakeupReadfd(int fd)
        {
            {
                std::lock_guard<spinlock> lock(ReadSocketLock);
                ReadSockets.push_back(fd);
            }
            eventfd_write(EventFd, 1);
        }
        void wakeupWritefd(int fd)
        {
            {
                std::lock_guard<spinlock> lock(WriteSocketLock);
                WriteSockets.push_back(fd);
            }
            eventfd_write(EventFd, 1);
        }

#endif
        int IncrementPendingIO() { return PendingIO.fetch_add(1, std::memory_order_acquire) + 1; }
        int DecrementPendingIO() { return PendingIO.fetch_sub(1, std::memory_order_acquire) - 1; }
        int getPendingIO() const { return PendingIO.load(std::memory_order_relaxed); }
        void stop() { KeepGoing_ = false; }
        void RegisterSocket(const SocketHandle &) {}
        Win_IO_Context &getWriteContext(const SocketHandle &socket)
        {
            auto index = socket.value;
            assert(index >= 0 && index < static_cast<decltype(index)>(WriteContexts.size()));
            return WriteContexts[index];
        }
        Win_IO_Context &getReadContext(const SocketHandle &socket)
        {
            auto index = socket.value;
            assert(index >= 0 && index < static_cast<decltype(index)>(ReadContexts.size()));
            return ReadContexts[index];
        }
        void DeregisterSocket(const SocketHandle &socket)
        {
            auto index = socket.value;
            if (index >= 0 && index < static_cast<decltype(index)>(ReadContexts.size())) {
                completeio(WriteContexts[index], *this, StatusCode::SC_CLOSED, 0);
                completeio(ReadContexts[index], *this, StatusCode::SC_CLOSED, 0);
            }
        }
    };

} // namespace NET
} // namespace SL
