#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;
namespace SL {
namespace NET {
    std::shared_ptr<IContext> CreateContext() { return std::make_shared<Context>(); }
    SOCKET BuildSocket(AddressFamily family)
    {
        SOCKET sock = INVALID_SOCKET;
        if (family == AddressFamily::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV4);
            INTERNAL::bind(sock, mytestaddr);
        }
        else {
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            bindaddr.sin6_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV6);
            INTERNAL::bind(sock, mytestaddr);
        }
        if (!INTERNAL::setsockopt_O_BLOCKING(sock, Blocking_Options::NON_BLOCKING)) {
            closesocket(sock);
            sock = INVALID_SOCKET;
        }
        return sock;
    }
    SOCKET Context::getSocket(AddressFamily family)
    {
        SOCKET sock = INVALID_SOCKET;
        {
            std::lock_guard<std::mutex> lock(ConnectionSocketsLock);
            if (!ConnectionSockets.empty()) {
                sock = ConnectionSockets.back();
                ConnectionSockets.pop_back();
            }
        }
        if (sock == INVALID_SOCKET) {
            sock = BuildSocket(family);
        }

        PendingIO += 1;
        auto sockcreate = new Win_IO_BuildConnectSocket_Context();
        sockcreate->IOOperation = IO_OPERATION::IoBuildConnectSocket;
        sockcreate->AddressFamily_ = family;
        if (PostQueuedCompletionStatus(iocp.handle, 0, (ULONG_PTR)this, (LPOVERLAPPED)&sockcreate->Overlapped) == FALSE) {
            PendingIO -= 1;
            delete sockcreate;
        }
        return sock;
    }

    Context::Context()
    {
        PendingIO = 0;
        if (!ConnectEx_) {
            auto handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
            GUID guid = WSAID_CONNECTEX;
            DWORD bytes = 0;
            WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &ConnectEx_, sizeof(ConnectEx_), &bytes, NULL, NULL);

            closesocket(handle);
        }
    }

    Context::~Context()
    {
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
        for (auto &a : ConnectionSockets) {
            closesocket(a);
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
    void Context::handle_buildconnectionsocket(bool success, Win_IO_BuildConnectSocket_Context *context)
    {
        if (success) {

            auto newsock = BuildSocket(context->AddressFamily_);
            std::lock_guard<std::mutex> lock(ConnectionSocketsLock);
            ConnectionSockets.push_back(newsock);
        }
        delete context;
    }
    void Context::run(ThreadCount threadcount)
    {

        Threads.reserve(threadcount.value);
        for (auto i = 0; i < threadcount.value; i++) {
            Threads.push_back(std::thread([&] {

                while (true) {
                    DWORD numberofbytestransfered = 0;
                    Win_IO_Context *overlapped = nullptr;
                    void *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(iocp.handle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&overlapped, INFINITE) == TRUE;

                    if (!overlapped) {
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
                        static_cast<Socket *>(completionkey)->increment_readbytes(numberofbytestransfered);
                        if (numberofbytestransfered == 0 && static_cast<Socket *>(completionkey)->ReadContext.bufferlen != 0 && bSuccess) {
                            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                        }
                        static_cast<Socket *>(completionkey)->continue_read(bSuccess);
                        break;
                    case IO_OPERATION::IoWrite:
                        static_cast<Socket *>(completionkey)->increment_writebytes(numberofbytestransfered);
                        if (numberofbytestransfered == 0 && static_cast<Socket *>(completionkey)->WriteContext.bufferlen != 0 && bSuccess) {
                            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
                        }
                        static_cast<Socket *>(completionkey)->continue_write(bSuccess);
                        break;
                    case IO_OPERATION::IoBuildConnectSocket:
                        handle_buildconnectionsocket(bSuccess, static_cast<Win_IO_BuildConnectSocket_Context *>(overlapped));
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