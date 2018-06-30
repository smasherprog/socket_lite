#include "Internal/Context.h"

namespace SL {
namespace NET {

    Context::Context(ThreadCount t) : ThreadCount_(t)
    {
        KeepGoing_ = true;
        PendingIO = 0;
        Threads.reserve(ThreadCount_.value);
        ReadContexts.resize(std::numeric_limits<unsigned short>::max());
        WriteContexts.resize(std::numeric_limits<unsigned short>::max());
        ReadContexts.shrink_to_fit();
        WriteContexts.shrink_to_fit();

#if _WIN32
        IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, ThreadCount_.value);
        if (IOCPHandle == NULL) {
            abort();
        }
        if (WSAStartup(0x202, &wsaData) != 0) {
            abort();
        }
        auto temphandle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        ConnectEx_ = nullptr;
        WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);
        if (ConnectEx_ == nullptr) {
            closesocket(temphandle);
            abort();
        }
        GUID acceptex_guid = WSAID_ACCEPTEX;
        bytes = 0;
        AcceptEx_ = nullptr;
        WSAIoctl(temphandle, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_, sizeof(AcceptEx_), &bytes, NULL,
                 NULL);
        if (AcceptEx_ == nullptr) {
            closesocket(temphandle);
            abort();
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
#endif
    }
    Context::~Context()
    {
        stop();
        Listeners.clear();
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
            std::this_thread::sleep_for(100ms);
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
    void Context::run()
    {
#if _WIN32
        for (decltype(ThreadCount::value) i = 0; i < ThreadCount_.value; i++) {
            Threads.emplace_back(std::thread([&] {
                for (;;) {
                    DWORD numberofbytestransfered = 0;
                    RW_Context *overlapped = nullptr;
                    void *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(IOCPHandle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE &&
                                    KeepGoing_;
                    if (overlapped == NULL) {
                        wakeup();
                        return;
                    }
                    switch (overlapped->IOOperation) {

                    case IO_OPERATION::IoRead:
                    case IO_OPERATION::IoWrite:
                        overlapped->remaining_bytes -= numberofbytestransfered;
                        overlapped->buffer += numberofbytestransfered; // advance the start of the buffer
                        if (numberofbytestransfered == 0 && bSuccess) {
                            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                        }
                        continue_io(bSuccess, *overlapped, *this, SocketHandle(reinterpret_cast<decltype(SocketHandle::value)>(completionkey)));
                        break;
                    case IO_OPERATION::IoConnect:
                        continue_connect(bSuccess, *overlapped, *this, SocketHandle(reinterpret_cast<decltype(SocketHandle::value)>(completionkey)));
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

        for (decltype(ThreadCount::value) i = 0; i < ThreadCount_.value; i++) {
            Threads.emplace_back(std::thread([&] {
                epoll_event epollevents[128];
                std::vector<int> socketbuffer;
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
                            auto &rctx = getWriteContext(handle);
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

#if _WIN32
    HANDLE Context::getIOHandle() const { return IOCPHandle; }
    void Context::wakeup() { PostQueuedCompletionStatus(IOCPHandle, 0, (DWORD)NULL, NULL); }
#else
    int Context::getIOHandle() const { return IOCPHandle; }
    void Context::wakeup() { eventfd_write(EventWakeFd, 1); }
    void Context::wakeupReadfd(int fd)
    {
        {
            std::lock_guard<spinlock> lock(ReadSocketLock);
            ReadSockets.push_back(fd);
        }
        eventfd_write(EventFd, 1);
    }
    void Context::wakeupWritefd(int fd)
    {
        {
            std::lock_guard<spinlock> lock(WriteSocketLock);
            WriteSockets.push_back(fd);
        }
        eventfd_write(EventFd, 1);
    }

#endif
    void Context::RegisterListener(std::shared_ptr<Listener> l) { Listeners.push_back(l); }

    void Context::RegisterSocket(const SocketHandle &) {}
    RW_Context &Context::getWriteContext(const SocketHandle &socket)
    {
        auto index = socket.value;
        assert(index >= 0 && index < static_cast<decltype(index)>(WriteContexts.size()));
        return WriteContexts[index];
    }
    RW_Context &Context::getReadContext(const SocketHandle &socket)
    {
        auto index = socket.value;
        assert(index >= 0 && index < static_cast<decltype(index)>(ReadContexts.size()));
        return ReadContexts[index];
    }
    void Context::DeregisterSocket(const SocketHandle &socket)
    {
        auto index = socket.value;
        if (index >= 0 && index < static_cast<decltype(index)>(ReadContexts.size())) {
            completeio(WriteContexts[index], *this, StatusCode::SC_CLOSED);
            completeio(ReadContexts[index], *this, StatusCode::SC_CLOSED);
        }
    }

    std::shared_ptr<Context> NetworkConfig::run()
    {
        Context_->run();
        return Context_;
    }
    std::shared_ptr<INetworkConfig> createContext(ThreadCount threadcount)
    {
        return std::make_shared<NetworkConfig>(std::make_shared<Context>(threadcount));
    }
} // namespace NET
} // namespace SL
