#pragma once

#include "Internal/spinlock.h"
#include "Socket_Lite.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory> 
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

    class Socket final : ISocket {
      public:
        PlatformSocket PlatformSocket_;
        IOData &IOData_;
        Win_IO_Context ReadContext_, WriteContext_;
        SocketStatus Status_;

        Socket(IOData &, PlatformSocket &&);
        Socket(Context &, PlatformSocket &&);
        Socket(Socket &&);
        Socket(Context &);
        Socket(IOData &);
        virtual ~Socket();
        virtual PlatformSocket &Handle() override { return PlatformSocket_; }
        virtual SocketStatus Status() const override { return Status_; }
        virtual void close() override;

        // guarantees async behavior and will not complete immediatly, but some time later
        virtual void recv_async(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler) override;
        virtual void send_async(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler) override;
    };
#if _WIN32
    void continue_io(bool success, Win_IO_Context *context);
#else 
    void continue_io(bool success, Win_IO_Context *context, const std::shared_ptr<Socket> &socket);
#endif
    void continue_connect(bool success, Win_IO_Context *context);
    class IOData {

        std::atomic<int> &PendingIO;
        bool KeepGoing_;
        std::thread Thread;

#if _WIN32
        HANDLE IOCPHandle;
#else
        std::vector<std::weak_ptr<Socket>> &Sockets;
        int EventWakeFd;
        int IOCPHandle;
#endif
      public:
#if _WIN32
        LPFN_CONNECTEX ConnectEx_;
        IOData(HANDLE h, LPFN_CONNECTEX c, std::atomic<int> &iocount) : PendingIO(iocount), IOCPHandle(h), ConnectEx_(c)
        {

            KeepGoing_ = true;
            Thread = std::thread([&] {
                while (true) {
                    DWORD numberofbytestransfered = 0;
                    Win_IO_Context *overlapped = nullptr;
                    void *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(IOCPHandle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;
                    if (overlapped == NULL) {
                        wakeup();
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
                    if (getPendingIO() <= 0 && !KeepGoing_) {
                        wakeup();
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
            Thread.join();
        }
        HANDLE getIOHandle() const { return IOCPHandle; }
        void wakeup() { PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL); }
#else

        IOData(std::atomic<int> &iocount, std::vector<std::weak_ptr<Socket>> &socks) : PendingIO(iocount), Sockets(socks)
        {

            KeepGoing_ = true;
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
            Thread = std::thread([&] {
                  epoll_event epollevents[128];
             
                    while (true) {
                        auto count = epoll_wait(IOCPHandle, epollevents, 128, -1);

                        for (auto i = 0; i < count; i++) {
                            if (epollevents[i].data.fd != EventWakeFd) {
                                auto socketclosed = epollevents[i].events & EPOLLERR || epollevents[i].events & EPOLLHUP;
                                auto s = getSocket(reinterpret_cast<Socket *>(epollevents[i].data.ptr));
                                if (!s) {
                                    continue;
                                }
                                if (s->Status() == SocketStatus::CONNECTING) {
                                    continue_connect(!socketclosed, &s->ReadContext_);
                                }
                                else if (epollevents[i].events & EPOLLIN) {
                                    continue_io(!socketclosed, &s->ReadContext_, s);
                                } else {
                                    continue_io(!socketclosed, &s->WriteContext_, s);
                                }
                            }
                        }
                        if (getPendingIO() <= 0 && !KeepGoing_) {
                            wakeup();
                            return;
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
            ::close(EventWakeFd);
            ::close(IOCPHandle);
        }
        int getIOHandle() const { return IOCPHandle; }
        void wakeup() { eventfd_write(EventWakeFd, 1); }
#endif
        int IncrementPendingIO() { return PendingIO.fetch_add(1, std::memory_order_acquire) + 1; }
        int DecrementPendingIO() { return PendingIO.fetch_sub(1, std::memory_order_acquire) - 1; }
        int getPendingIO() const { return PendingIO.load(std::memory_order_relaxed); }
        void stop() { KeepGoing_ = false; }

#if _WIN32
        void RegisterSocket(std::shared_ptr<Socket> &) {}
        std::shared_ptr<Socket> getSocket(Socket *) { return std::shared_ptr<Socket>(); }
        void DeregisterSocket(const Socket *) {}
#else
        void RegisterSocket(const std::shared_ptr<Socket> &socket) {  
            auto index = socket->PlatformSocket_.Handle().value;
            if(index >=0 && index< static_cast<decltype(index)>(Sockets.size())) {
                Sockets[index] = socket; 
            } 
        }
        std::shared_ptr<Socket> getSocket(Socket *socket) {    
            if(!socket) return std::shared_ptr<Socket>(); 
            auto index = socket->PlatformSocket_.Handle().value;
            if(index >=0 && index<static_cast<decltype(index)>(Sockets.size())) {
                return Sockets[index].lock();    
            }
            return std::shared_ptr<Socket>();
        }
        void DeregisterSocket(const Socket *socket) {    
            auto index = socket->PlatformSocket_.Handle().value;
            if(index >=0 && index<static_cast<decltype(index)>(Sockets.size())) {
                Sockets[index].reset();  
            }  
        }
#endif
    };
    class ContextImpl {
        const ThreadCount ThreadCount_;
        std::vector<std::unique_ptr<IOData>> ThreadData;
        std::vector<std::weak_ptr<Socket>> Sockets;
        int LastThreadIndex;
        std::atomic<int> PendingIO;
#if _WIN32
        HANDLE IOCPHandle;
        WSADATA wsaData;
#endif

      public:
        ContextImpl(ThreadCount t) : ThreadCount_(t)
        {
            PendingIO = 0;
            ThreadData.resize(ThreadCount_.value);

#if _WIN32

            IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, t.value);
            if (WSAStartup(0x202, &wsaData) != 0) {
                abort();
            }
            LPFN_CONNECTEX connectex;
            auto handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
            GUID guid = WSAID_CONNECTEX;
            DWORD bytes = 0;
            WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &connectex, sizeof(connectex), &bytes, NULL, NULL);
            closesocket(handle);
            if (connectex == nullptr) {
                abort();
            }
            for (auto &th : ThreadData) {
                th = std::make_unique<IOData>(IOCPHandle, connectex, PendingIO);
            }
#else
            Sockets.resize(std::numeric_limits<unsigned short>::max());
            for (auto &th : ThreadData) {
                th = std::make_unique<IOData>(PendingIO, Sockets);
            }
#endif

            LastThreadIndex = 0;
        }
        ~ContextImpl()
        {
            for (auto &t : ThreadData) {
                t->stop();
            }

            ThreadData.clear(); // force release here
            Sockets.clear();
#if _WIN32
            CloseHandle(IOCPHandle);
            WSACleanup();
#endif
        }
        IOData &getIOData() { return *ThreadData[LastThreadIndex++ % ThreadCount_.value]; }
    };
} // namespace NET 
}