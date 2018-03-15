#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;
namespace SL {
namespace NET {
    std::shared_ptr<IContext> CreateContext(ThreadCount threadcount) { return std::make_shared<Context>(threadcount); }

    Context::Context(ThreadCount threadcount)
        : ThreadCount_(threadcount), Win_IO_RW_ContextBuffer(), RW_CompletionHandlerBuffer(), iocp(threadcount.value)
    {
        PendingIO = 0;
        if (!ConnectEx_) {
            auto handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
            GUID guid = WSAID_CONNECTEX;
            DWORD bytes = 0;
            WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);
            closesocket(handle);
        }

        SocketBuiler = std::thread([&] {
            std::vector<SOCKET> tmpipv4;
            std::vector<SOCKET> tmpipv6;

            while (!StopBuildingSockets) {
                auto ipv4socks = Ipv4SocketBuffer.size();
                auto ipv6socks = Ipv6SocketBuffer.size();
                auto waittime = std::min(ipv4socks, ipv6socks) * 10;
                for (auto i = ipv4socks; i < 10; i++) {
                    tmpipv4.push_back(INTERNAL::Socket(AddressFamily::IPV4));
                }
                for (auto i = ipv6socks; i < 10; i++) {
                    tmpipv6.push_back(INTERNAL::Socket(AddressFamily::IPV6));
                }
                {
                    std::lock_guard<spinlock> lock(SocketBufferLock);
                    for (auto a : tmpipv4) {
                        Ipv4SocketBuffer.push_back(a);
                    }
                    for (auto a : tmpipv6) {
                        Ipv6SocketBuffer.push_back(a);
                    }
                }
                tmpipv6.clear();
                tmpipv4.clear();
                std::this_thread::sleep_for(std::chrono::milliseconds(waittime));
            }
        });
    }

    Context::~Context()
    {
        StopBuildingSockets = true;
        SocketBuiler.join();
        {
            std::lock_guard<spinlock> lock(SocketBufferLock);
            for (auto a : Ipv6SocketBuffer) {
                closesocket(a);
            }
            for (auto a : Ipv4SocketBuffer) {
                closesocket(a);
            }
        }
        while (PendingIO > 0) {
            std::this_thread::sleep_for(5ms);
        }

        PostQueuedCompletionStatus(iocp.handle, 0, (DWORD)NULL, NULL);

        for (auto &t : Threads) {

            if (t.joinable()) {
                // destroying myself
                if (t.get_id() == std::this_thread::get_id()) {
                    t.detach();
                }
                else {
                    t.join();
                }
            }
        }
    }
    SOCKET Context::getSocket(AddressFamily family)
    {
        // return INTERNAL::Socket(family);
        if (family == AddressFamily::IPV4) {
            if (Ipv4SocketBuffer.empty()) {
                return INTERNAL::Socket(AddressFamily::IPV4);
            }
            SOCKET s = INVALID_SOCKET;
            {
                std::lock_guard<spinlock> lock(SocketBufferLock);
                if (!Ipv4SocketBuffer.empty()) {
                    s = Ipv4SocketBuffer.back();
                    Ipv4SocketBuffer.pop_back();
                }
            }
            if (s == INVALID_SOCKET) {
                s = INTERNAL::Socket(AddressFamily::IPV4);
            }
            return s;
        }
        else {
            if (Ipv6SocketBuffer.empty()) {
                return INTERNAL::Socket(AddressFamily::IPV6);
            }
            SOCKET s = INVALID_SOCKET;
            {
                std::lock_guard<spinlock> lock(SocketBufferLock);
                if (!Ipv6SocketBuffer.empty()) {
                    s = Ipv6SocketBuffer.back();
                    Ipv6SocketBuffer.pop_back();
                }
            }
            if (s == INVALID_SOCKET) {
                s = INTERNAL::Socket(AddressFamily::IPV6);
            }
            return s;
        }
    }
    std::shared_ptr<ISocket> Context::CreateSocket() { return std::make_shared<Socket>(this); }
    std::shared_ptr<IListener> Context::CreateListener(std::shared_ptr<ISocket> &&listensocket)
    {
        auto addr = listensocket->getsockname();
        if (!addr.has_value()) {
            return std::shared_ptr<IListener>();
        }
        auto listener = std::make_shared<Listener>(this, std::forward<std::shared_ptr<ISocket>>(listensocket), addr.value());

        if (!Socket::UpdateIOCP(listener->ListenSocket->get_handle(), &iocp.handle, listener.get())) {
            return std::shared_ptr<IListener>();
        }
        return listener;
    }
    void Context::run()
    {
        Threads.reserve(ThreadCount_.value);
        for (auto i = 0; i < ThreadCount_.value; i++) {
            Threads.push_back(std::thread([&] {
                std::vector<SOCKET> socketbuffer;
                while (true) {
                    DWORD numberofbytestransfered = 0;
                    Win_IO_Context *overlapped = nullptr;
                    void *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(iocp.handle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;

                    if (PendingIO <= 0) {
                        PostQueuedCompletionStatus(iocp.handle, 0, (DWORD)NULL, NULL);
                        return;
                    }
                    switch (overlapped->IOOperation) {

                    case IO_OPERATION::IoInitConnect:
                        static_cast<Socket *>(completionkey)->init_connect(bSuccess, static_cast<Win_IO_Connect_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoConnect:
                        static_cast<Socket *>(completionkey)->continue_connect(bSuccess, static_cast<Win_IO_Connect_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoStartAccept:
                        static_cast<Listener *>(completionkey)->start_accept(bSuccess, static_cast<Win_IO_Accept_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoAccept:
                        static_cast<Listener *>(completionkey)->handle_accept(bSuccess, static_cast<Win_IO_Accept_Context *>(overlapped));
                        break;
                    case IO_OPERATION::IoRead:
                    case IO_OPERATION::IoWrite:
                        static_cast<Win_IO_RW_Context *>(overlapped)->transfered_bytes += numberofbytestransfered;
                        if (numberofbytestransfered == 0 && static_cast<Win_IO_RW_Context *>(overlapped)->bufferlen != 0 && bSuccess) {
                            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                        }
                        static_cast<Socket *>(completionkey)->continue_io(bSuccess, static_cast<Win_IO_RW_Context *>(overlapped));
                        break;
                    default:
                        break;
                    }

                    if (--PendingIO <= 0) {
                        PostQueuedCompletionStatus(iocp.handle, 0, (DWORD)NULL, NULL);
                        return;
                    }
                }
            }));
        }
    }
} // namespace NET
} // namespace SL