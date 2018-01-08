
#include "ListenContext.h"
#include "Socket.h"
#include "internal/List.h"
#include <iostream>
namespace SL {
namespace NET {

    ListenContext::ListenContext() {}
    ListenContext::~ListenContext()
    {
        KeepRunning = false;
        for (size_t i = 0; i < Threads.size(); i++) {
            // Help threads get out of blocking - GetQueuedCompletionStatus()
            PostQueuedCompletionStatus(iocp.handle, 0, (DWORD)NULL, NULL);
        }
        if (ListenIOContext.IO_Context.Socket_) {
            ListenIOContext.IO_Context.Socket_.reset();
        }
        if (ListenSocket != INVALID_SOCKET) {
            closesocket(ListenSocket);
        }

        while (!HasOverlappedIoCompleted((LPOVERLAPPED)&ListenIOContext.Overlapped))
            Sleep(0);
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
        auto head = Clients;
        while (head) {
            auto tmp = head;
            if (tmp->ReadContext) {
                tmp->read_complete(-1, tmp->ReadContext.get());
            }
            tmp->write_complete(-1, nullptr);
            head = pop_front(&Clients);
        }
    }
    bool ListenContext::bind(PortNumber port)
    {
        if (auto sock = create_and_bind(port); sock.has_value()) {
            ListenSocket = *sock;
            sockaddr_storage addr;
            socklen_t len = sizeof(addr);
            if (getpeername(ListenSocket, (sockaddr *)&addr, &len) == 0) {
                AddressFamily = addr.ss_family;
            }
            return make_socket_non_blocking(*sock);
        }
        return false;
    }
    bool ListenContext::listen()
    {
        if (ListenSocket != INVALID_SOCKET) {
            if (::listen(ListenSocket, 5) != SOCKET_ERROR) {
                GUID acceptex_guid = WSAID_ACCEPTEX;
                DWORD bytes = 0;
                if (WSAIoctl(ListenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_, sizeof(AcceptEx_),
                             &bytes, NULL, NULL) != SOCKET_ERROR) {
                    if (iocp.handle = CreateIoCompletionPort((HANDLE)ListenSocket, iocp.handle, (DWORD_PTR)&ListenIOContext.IO_Context, 0);
                        iocp.handle != NULL) {
                        return true;
                    }
                    else {
                        std::cerr << "CreateIoCompletionPort() failed: " << GetLastError() << std::endl;
                    }
                }
                else {
                    std::cerr << "failed to load AcceptEx: " << WSAGetLastError() << std::endl;
                }
            }
        }
        return false;
    }
    void ListenContext::closeclient(Socket *socket, Win_IO_Context *context)
    {
        if (socket->next || socket->prev) { // it could have already been removed.. check to prevent a lock
            std::lock_guard<SpinLock> lock(ClientLock);
            remove(&Clients, socket);
        }
        if (context->IOOperation == IO_OPERATION::IoRead) {
            socket->read_complete(-1, context);
        }
        else if (context->IOOperation == IO_OPERATION::IoWrite) {
            socket->write_complete(-1, static_cast<Win_IO_Context_List *>(context));
        }
    }
    void ListenContext::async_accept()
    {
        ListenIOContext.IO_Context.Socket_ = std::make_shared<Socket>();
        ListenIOContext.IO_Context.Socket_->handle = WSASocketW(AddressFamily, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        DWORD recvbytes = 0;
        auto nRet = AcceptEx_(ListenSocket, ListenIOContext.IO_Context.Socket_->handle, (LPVOID)(AcceptBuffer), 0, sizeof(SOCKADDR_STORAGE) + 16,
                              sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, (LPOVERLAPPED) & (ListenIOContext.Overlapped));

        if (auto wsaerr = WSAGetLastError(); nRet == SOCKET_ERROR && (ERROR_IO_PENDING != wsaerr)) {
            std::cerr << "Error AcceptEx_ Code: " << wsaerr << std::endl;
        }
    }
    void ListenContext::run(ThreadCount threadcount)
    {
        async_accept(); // start waiting for a new connection
        Threads.reserve(threadcount.value);
        for (auto i = 0; i < threadcount.value; i++) {
            Threads.push_back(std::thread([&] {

                while (KeepRunning) {
                    DWORD numberofbytestransfered = 0;
                    Win_IO_Context *lpOverlapped = nullptr;
                    Socket *completionkey = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(iocp.handle, &numberofbytestransfered, (PDWORD_PTR)&completionkey,
                                                              (LPOVERLAPPED *)&lpOverlapped, 100);
                    if (!KeepRunning) {
                        return;
                    }
                    if (bSuccess == FALSE && lpOverlapped == NULL) {
                        if (GetLastError() == ERROR_ABANDONED_WAIT_0) {
                            return; // the iocp handle was closed!
                        }
                        continue; // timer ran out go back to top and try again
                    }

                    if (lpOverlapped->IOOperation != IO_OPERATION::IoAccept &&
                        (bSuccess == FALSE || (bSuccess == TRUE && 0 == numberofbytestransfered))) {
                        // dropped connection
                        closeclient(completionkey, lpOverlapped);
                        async_accept(); // start waiting for a new connection
                        continue;
                    }
                    switch (lpOverlapped->IOOperation) {
                    case IO_OPERATION::IoAccept:

                        if (setsockopt(lpOverlapped->IO_Context.Socket_->handle, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&ListenSocket,
                                       sizeof(ListenSocket)) == SOCKET_ERROR) {
                            std::cerr << "Error setsockopt SO_UPDATE_ACCEPT_CONTEXT Code: " << WSAGetLastError() << std::endl;
                            return;
                        }
                        if (auto ctx =
                                CreateIoCompletionPort((HANDLE)lpOverlapped->IO_Context.Socket_->handle, iocp.handle, (DWORD_PTR)completionkey, 0);
                            ctx == NULL) {
                            std::cerr << "Error setsockopt SO_UPDATE_ACCEPT_CONTEXT Code: " << WSAGetLastError() << std::endl;
                            return;
                        }
                        onConnection(std::move(lpOverlapped->IO_Context.Socket_));
                        async_accept(); // start waiting for a new connection
                        break;
                    case IO_OPERATION::IoRead:
                        lpOverlapped->IO_Context.transfered_bytes += numberofbytestransfered;
                        completionkey->continue_read(lpOverlapped);
                        if (completionkey->handle == INVALID_SOCKET) {
                            closeclient(completionkey, lpOverlapped);
                        }
                        break;
                    case IO_OPERATION::IoWrite:
                        lpOverlapped->IO_Context.transfered_bytes += numberofbytestransfered;
                        completionkey->continue_write(static_cast<Win_IO_Context_List *>(lpOverlapped));
                        if (completionkey->handle == INVALID_SOCKET) {
                            closeclient(completionkey, lpOverlapped);
                        }
                        break;
                    default:
                        break;
                    }
                }
            }));
        }
    }

} // namespace NET
} // namespace SL