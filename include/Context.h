#pragma once
#include "defs.h"
#include "spinlock.h"
namespace SL::NET {
// forward declare
class Socket;
template <class T> class Listener;
class Context {
    const ThreadCount ThreadCount_;
    std::vector<std::thread> Threads;
    std::vector<RW_Context> ReadContexts, WriteContexts;
    bool KeepGoing_;
    std::atomic<int> PendingIO;
    std::vector<SocketHandle> ReadSockets, WriteSockets;
    spinlock ReadSocketLock, WriteSocketLock;

#if _WIN32
    HANDLE IOCPHandle;
    WSADATA wsaData;
    LPFN_CONNECTEX ConnectEx_;
    LPFN_ACCEPTEX AcceptEx_;
    void wakeup() { wakeup(IOCPHandle); }
    static void wakeup(const HANDLE h)
    {
        if (h != NULL) {
            PostQueuedCompletionStatus(h, 0, (DWORD)NULL, NULL);
        }
    }
    HANDLE getIOHandle() const { return IOCPHandle; }
    void wakeupReadfd(SocketHandle fd)
    {
        {
            std::lock_guard<spinlock> lock(ReadSocketLock);
            ReadSockets.push_back(fd);
        }
        PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL);
    }
    void wakeupWritefd(SocketHandle fd)
    {
        {
            std::lock_guard<spinlock> lock(WriteSocketLock);
            WriteSockets.push_back(fd);
        }
        PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL);
    }
#else
    int EventWakeFd;
    int EventFd;
    int IOCPHandle;
    int getIOHandle() const { return IOCPHandle; }
    void wakeup()
    {
        if (EventWakeFd > 0) {
            eventfd_write(EventWakeFd, 1);
        }
    }
    void wakeupReadfd(SocketHandle fd)
    {
        {
            std::lock_guard<spinlock> lock(ReadSocketLock);
            ReadSockets.push_back(fd);
        }
        eventfd_write(EventFd, 1);
    }
    void wakeupWritefd(SocketHandle fd)
    {
        {
            std::lock_guard<spinlock> lock(WriteSocketLock);
            WriteSockets.push_back(fd);
        }
        eventfd_write(EventFd, 1);
    }

#endif
    void HandlePendingIO(std::vector<SocketHandle> &socketbuffer)
    {
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
                auto &rctx = getWriteContext(handle);
                continue_io(true, rctx, *this, handle);
            }
            socketbuffer.clear();
        }
    }

  public:
    Context(ThreadCount t) : ThreadCount_(t)
    {
        KeepGoing_ = true;
       
        PendingIO = 0;
        Threads.reserve(ThreadCount_.value * 2);
        ReadContexts.resize(std::numeric_limits<unsigned short>::max());
        WriteContexts.resize(std::numeric_limits<unsigned short>::max());
        ReadContexts.shrink_to_fit();
        WriteContexts.shrink_to_fit();

#if _WIN32
        IOCPHandle = nullptr;
        ConnectEx_ = nullptr;
        AcceptEx_ = nullptr;
        if (WSAStartup(0x202, &wsaData) != 0) {
            abort();
        }
        IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, ThreadCount_.value);
        assert(IOCPHandle != NULL);
        auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        ConnectEx_ = nullptr;
        WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);
        assert(ConnectEx_ != nullptr);
        GUID acceptex_guid = WSAID_ACCEPTEX;
        bytes = 0;
        AcceptEx_ = nullptr;
        WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_, sizeof(AcceptEx_), &bytes, NULL,
                 NULL);
        assert(AcceptEx_ != nullptr);
#else
        IOCPHandle = 0;
        EventWakeFd = EventFd = 0;
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
#endif
    }
    ~Context() { stop(); }
    void start()
    {
        assert(KeepGoing_);
#if _WIN32
        for (decltype(ThreadCount::value) i = 0; i < ThreadCount_.value; i++) {
            Threads.emplace_back(std::thread([&] {
                std::vector<SocketHandle> socketbuffer;
                for (;;) {
                    DWORD numberofbytestransfered = 0;
                    RW_Context *overlapped = nullptr;
                    void *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(IOCPHandle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE &&
                                    KeepGoing_;
                    if (overlapped != nullptr) {
                        switch (auto eventyype = overlapped->getEvent()) {
                        case IO_OPERATION::IoRead:
                        case IO_OPERATION::IoWrite:

                            overlapped->setRemainingBytes(overlapped->getRemainingBytes() - numberofbytestransfered);
                            overlapped->buffer += numberofbytestransfered; // advance the start of the buffer
                            if (numberofbytestransfered == 0 && bSuccess) {
                                bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                            }
                            continue_io(bSuccess, *overlapped, *this, SocketHandle(reinterpret_cast<decltype(SocketHandle::value)>(completionkey)));
                            break;
                        case IO_OPERATION::IoConnect:
                            continue_connect(bSuccess, *overlapped, *this,
                                             SocketHandle(reinterpret_cast<decltype(SocketHandle::value)>(completionkey)));
                            break;
                        default:
                            break;
                        }
                    }

                    if (getPendingIO() <= 0 && !KeepGoing_) {
                        wakeup();
                        return;
                    }
                }
            }));
        }
#else

        for (decltype(ThreadCount::value) i = 0; i < ThreadCount_.value; i++) {
            Threads.emplace_back(std::thread([&] {
                epoll_event epollevents[128];
                std::vector<SocketHandle> socketbuffer;
                for (;;) {
                    auto count = epoll_wait(IOCPHandle, epollevents, 128, -1);

                    for (auto i = 0; i < count; i++) {
                        if (epollevents[i].data.fd != EventWakeFd && epollevents[i].data.fd != EventFd) {
                            auto socketclosed = ((epollevents[i].events & EPOLLERR) || (epollevents[i].events & EPOLLHUP)) && KeepGoing_;

                            SocketHandle handle(epollevents[i].data.fd);

                            if (epollevents[i].events & EPOLLIN) {
                                auto &rctx = getReadContext(handle);
                                continue_io(!socketclosed, rctx, *this, handle);
                            }
                            else {
                                auto &wctx = getWriteContext(handle);
                                if (wctx.getEvent() == IO_OPERATION::IoConnect) {
                                    continue_connect(!socketclosed, wctx, *this, handle);
                                }
                                else if (wctx.getEvent() == IO_OPERATION::IoWrite) {
                                    continue_io(!socketclosed, wctx, *this, handle);
                                }
                            }
                        }
                    }
                    HandlePendingIO(socketbuffer);
                    if (getPendingIO() <= 0 && !KeepGoing_) {
                        wakeup();
                        return;
                    }
                }
            }));
        }
#endif
    }
    void stop()
    {
        KeepGoing_ = false;
#ifndef _WIN32
        while (getPendingIO() > 0) {

            for (auto &rcts : ReadContexts) {
                completeio(rcts, *this, StatusCode::SC_CLOSED);
            }
            for (auto &rcts : WriteContexts) {
                completeio(rcts, *this, StatusCode::SC_CLOSED);
            }
            std::this_thread::sleep_for(100ms);
            wakeup();
        }
#else
        while (getPendingIO() > 0) {

            for (auto &rcts : ReadContexts) {
                completeio(rcts, *this, StatusCode::SC_CLOSED);
            }
            for (auto &rcts : WriteContexts) {
                completeio(rcts, *this, StatusCode::SC_CLOSED);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
        if (IOCPHandle != nullptr) {
            CloseHandle(IOCPHandle);
            WSACleanup();
            IOCPHandle = nullptr;
        }
#else
        ::close(EventFd);
        ::close(EventWakeFd);
        ::close(IOCPHandle);
#endif
    }

  protected:
    unsigned int getThreadCount() const { return ThreadCount_.value; }
    int IncrementPendingIO() { return PendingIO.fetch_add(1, std::memory_order_acquire) + 1; }
    int DecrementPendingIO() { return PendingIO.fetch_sub(1, std::memory_order_acquire) - 1; }
    int getPendingIO() const { return PendingIO.load(std::memory_order_relaxed); }

    RW_Context &getWriteContext(const SocketHandle &socket)
    {
        auto index = socket.value;
        assert(index >= 0 && index < static_cast<decltype(index)>(WriteContexts.size()));
        return WriteContexts[index];
    }
    RW_Context &getReadContext(const SocketHandle &socket)
    {
        auto index = socket.value;
        assert(index >= 0 && index < static_cast<decltype(index)>(ReadContexts.size()));
        return ReadContexts[index];
    }
    void DeregisterSocket(const SocketHandle &socket)
    {
        auto index = socket.value;
        if (index >= 0 && index < static_cast<decltype(index)>(ReadContexts.size())) {
            completeio(WriteContexts[index], *this, StatusCode::SC_CLOSED);
            completeio(ReadContexts[index], *this, StatusCode::SC_CLOSED);
        }
    }
    friend class Socket;
    template <class T> friend class Listener;
    friend void SL::NET::connect_async(Socket &, SocketAddress &, const SocketHandler &);
    friend void SL::NET::setup(RW_Context &, Context &, IO_OPERATION, int, unsigned char *, const SocketHandler &);
    friend void SL::NET::completeio(RW_Context &, Context &, StatusCode);
    friend void SL::NET::continue_io(bool success, RW_Context &context, Context &iodata, const SocketHandle &handle);
};

} // namespace SL::NET
