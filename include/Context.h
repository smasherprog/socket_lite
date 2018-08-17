#pragma once
#include "Internal.h"
#include "PlatformSocket.h"
#include "spinlock.h"
#include <mutex>

namespace SL::NET {

class Context {

    friend class AsyncPlatformSocket;
    friend class AsyncPlatformAcceptor;

  protected:
    const ThreadCount ThreadCount_;
    std::vector<std::thread> Threads;
    std::vector<RW_Context> ReadContexts, WriteContexts;
    bool KeepGoing_;
    std::atomic<int> PendingIO;
    std::vector<SocketHandle> ReadSockets, WriteSockets;
    spinlock ReadSocketLock, WriteSocketLock;

    void continue_recv_async(SocketHandle handle, RW_Context &rwcontext)
    {
        if (rwcontext.getRemainingBytes() <= 0) {
            return INTERNAL::completeio(rwcontext, PendingIO, StatusCode::SC_SUCCESS);
        }
        auto count = INTERNAL::Recv(handle, rwcontext.buffer, rwcontext.getRemainingBytes());
        if (count == rwcontext.getRemainingBytes()) {
            INTERNAL::completeio(rwcontext, PendingIO, StatusCode::SC_SUCCESS);
        }
        else if (count > 0) {
            rwcontext.setRemainingBytes(rwcontext.getRemainingBytes() - count);
            rwcontext.buffer += count;
#if _WIN32
            WSABUF wsabuf;
            wsabuf.buf = (char *)rwcontext.buffer;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(rwcontext.getRemainingBytes());
            DWORD dwSendNumBytes(0), dwFlags(0);
            DWORD nRet = WSARecv(handle.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(rwcontext.Overlapped), NULL);
            if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                INTERNAL::completeio(rwcontext, PendingIO, TranslateError(&lasterr));
            }
#else
            PostDeferredReadIO(Handle);
#endif
        }
        else if (INTERNAL::wouldBlock()) {
#if _WIN32
            WSABUF wsabuf;
            wsabuf.buf = (char *)rwcontext.buffer;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(rwcontext.getRemainingBytes());
            DWORD dwSendNumBytes(0), dwFlags(0);
            DWORD nRet = WSARecv(handle.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(rwcontext.Overlapped), NULL);
            if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                INTERNAL::completeio(rwcontext, PendingIO, TranslateError(&lasterr));
            }
#else
            epoll_event ev = {0};
            ev.data.fd = handle.value;
            ev.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
            if (epoll_ctl(IOCPHandle, EPOLL_CTL_MOD, handle.value, &ev) == -1) {
                INTERNAL::completeio(rwcontext, PendingIO, TranslateError());
            }
#endif
        }
        else {
            INTERNAL::completeio(rwcontext, PendingIO, TranslateError());
        }
    }

    void continue_send_async(SocketHandle handle, RW_Context &rwcontext)
    {
        if (rwcontext.getRemainingBytes() <= 0) {
            return INTERNAL::completeio(rwcontext, PendingIO, StatusCode::SC_SUCCESS);
        }
        auto count = INTERNAL::Send(handle, rwcontext.buffer, rwcontext.getRemainingBytes());
        if (count == rwcontext.getRemainingBytes()) {
            INTERNAL::completeio(rwcontext, PendingIO, StatusCode::SC_SUCCESS);
        }
        else if (count > 0) {
            rwcontext.setRemainingBytes(rwcontext.getRemainingBytes() - count);
            rwcontext.buffer += count;
#if _WIN32
            WSABUF wsabuf;
            wsabuf.buf = (char *)rwcontext.buffer;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(rwcontext.getRemainingBytes());
            DWORD dwSendNumBytes(0), dwFlags(0);
            DWORD nRet = WSASend(handle.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(rwcontext.Overlapped), NULL);
            if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                INTERNAL::completeio(rwcontext, PendingIO, TranslateError(&lasterr));
            }
#else
            PostDeferredWriteIO(Handle_);
#endif
        }
        else if (INTERNAL::wouldBlock()) {
#if _WIN32
            WSABUF wsabuf;
            wsabuf.buf = (char *)rwcontext.buffer;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(rwcontext.getRemainingBytes());
            DWORD dwSendNumBytes(0), dwFlags(0);
            DWORD nRet = WSASend(handle.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(rwcontext.Overlapped), NULL);
            if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                INTERNAL::completeio(rwcontext, PendingIO, TranslateError(&lasterr));
            }
#else
            epoll_event ev = {0};
            ev.data.fd = Handle_.value;
            ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
            if (epoll_ctl(context.getIOHandle(), EPOLL_CTL_MOD, handle.value, &ev) == -1) {
                INTERNAL::completeio(rwcontext, PendingIO, TranslateError());
            }
#endif
        }
        else {
            INTERNAL::completeio(rwcontext, PendingIO, TranslateError());
        }
    }

#if _WIN32
    HANDLE IOCPHandle;
    WSADATA wsaData;

    void wakeup() { wakeup(IOCPHandle); }
    static void wakeup(const HANDLE h)
    {
        if (h != NULL) {
            PostQueuedCompletionStatus(h, 0, (DWORD)NULL, NULL);
        }
    }
    HANDLE getIOHandle() const { return IOCPHandle; }
    void PostDeferredReadIO(SocketHandle fd)
    {
        {
            std::lock_guard<spinlock> lock(ReadSocketLock);
            ReadSockets.push_back(fd);
        }
        PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL);
    }
    void PostDeferredWriteIO(SocketHandle fd)
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
    void PostDeferredReadIO(SocketHandle fd)
    {
        {
            std::lock_guard<spinlock> lock(ReadSocketLock);
            ReadSockets.push_back(fd);
        }
        eventfd_write(EventFd, 1);
    }
    void PostDeferredWriteIO(SocketHandle fd)
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
                continue_recv_async(handle, rctx);
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
                continue_send_async(handle, rctx);
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
        if (WSAStartup(0x202, &wsaData) != 0) {
            abort();
        }
        IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, ThreadCount_.value);
        assert(IOCPHandle != NULL);
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
                        if (bSuccess) {
                            switch (auto eventyype = overlapped->getEvent()) {
                            case IO_OPERATION::IoRead:
                                overlapped->setRemainingBytes(overlapped->getRemainingBytes() - numberofbytestransfered);
                                overlapped->buffer += numberofbytestransfered; // advance the start of the buffer
                                continue_recv_async(reinterpret_cast<decltype(SocketHandle::value)>(completionkey), *overlapped);
                                break;
                            case IO_OPERATION::IoWrite:
                                overlapped->setRemainingBytes(overlapped->getRemainingBytes() - numberofbytestransfered);
                                overlapped->buffer += numberofbytestransfered; // advance the start of the buffer
                                continue_send_async(reinterpret_cast<decltype(SocketHandle::value)>(completionkey), *overlapped);

                                break;
                            case IO_OPERATION::IoConnect:
                            case IO_OPERATION::IoAccept:
                                INTERNAL::completeio(*overlapped, PendingIO, StatusCode::SC_SUCCESS);
                                break;
                            default:
                                break;
                            }
                        }
                        else {
                            INTERNAL::completeio(*overlapped, PendingIO, TranslateError());
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
                INTERNAL::completeio(rcts, PendingIO, StatusCode::SC_CLOSED);
            }
            for (auto &rcts : WriteContexts) {
                INTERNAL::completeio(rcts, PendingIO, StatusCode::SC_CLOSED);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wakeup();
        }
#endif

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
        if (EventFd != 0) {
            ::close(EventFd);
            EventFd = 0;
        }
        if (EventWakeFd != 0) {
            ::close(EventWakeFd);
            EventWakeFd = 0;
        }
        if (IOCPHandle != 0) {
            ::close(IOCPHandle);
            IOCPHandle = 0;
        }
#endif
    }

  protected:
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
            INTERNAL::completeio(WriteContexts[index], PendingIO, StatusCode::SC_CLOSED);
            INTERNAL::completeio(ReadContexts[index], PendingIO, StatusCode::SC_CLOSED);
        }
    }
}; // namespace SL::NET

} // namespace SL::NET
