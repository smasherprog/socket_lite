
#include "ListenContext.h"
#include "Socket.h"
namespace SL {
namespace NET {

    ListenContext::ListenContext() {}
    ListenContext::~ListenContext()
    {
        KeepRunning = false;
        if (ListenSocket != INVALID_SOCKET) {
            closesocket(ListenSocket);
        }

        for (size_t i = 0; i < Threads.size(); i++) {
            // Help threads get out of blocking - GetQueuedCompletionStatus()
            PostQueuedCompletionStatus(iocp.handle, 0, (DWORD)NULL, NULL);
        }
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
    bool ListenContext::bind(PortNumber port)
    {
        if (auto sock = create_and_bind(port); sock.has_value()) {
            ListenSocket = *sock;
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
                    return updateIOCP(ListenSocket, &iocp.handle);
                }
            }
        }
        return false;
    }
    void ListenContext::async_accept()
    {
        auto socket = std::make_shared<Socket>();
        socket->SendBuffers.push_back(PER_IO_CONTEXT());
        auto val = &socket->SendBuffers.front();
        val->Socket_ = socket;
        DWORD recvbytes = 0;
        auto nRet = AcceptEx_(ListenSocket, socket->handle, (LPVOID)(AcceptBuffer), 0, sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16,
                              &recvbytes, (LPOVERLAPPED) & (val->Overlapped));
        if (nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
        }
    }
    void ListenContext::run(ThreadCount threadcount)
    {
        async_accept(); // start waiting for a new connection
        Threads.reserve(threadcount.value);
        for (auto i = 0; i < threadcount.value; i++) {
            Threads.push_back(std::thread([&] {

                DWORD dwSendNumBytes(0);
                DWORD dwFlags(0);
                while (KeepRunning) {
                    DWORD numberofbytestransfered = 0;
                    PER_IO_CONTEXT *lpOverlapped = nullptr;
                    void *lpPerSocketContext = nullptr;

                    auto bSuccess = GetQueuedCompletionStatus(iocp.handle, &numberofbytestransfered, (PDWORD_PTR)&lpPerSocketContext,
                                                              (LPOVERLAPPED *)&lpOverlapped, 100);
                    if (!KeepRunning) {
                        // get out this thread is done meow!
                        return;
                    }
                    if (bSuccess == FALSE && lpOverlapped == NULL) {
                        continue; // timer ran out go back to top and try again
                    }
                    if (lpOverlapped->IOOperation != IO_OPERATION::IoAccept &&
                        (bSuccess == FALSE || (bSuccess == TRUE && 0 == numberofbytestransfered))) {
                        // dropped connection
                        lpOverlapped->Socket_->close();
                        onDisconnection(lpOverlapped->Socket_);
                        continue;
                    }
                    switch (lpOverlapped->IOOperation) {
                    case IO_OPERATION::IoAccept:
                        if (auto ret = setsockopt(lpOverlapped->Socket_->handle, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&ListenSocket,
                                                  sizeof(ListenSocket));
                            ret == SOCKET_ERROR) {
                            // big error here...
                            return;
                        }
                        if (!updateIOCP(lpOverlapped->Socket_->handle, &iocp.handle)) {
                            // big error here...
                            return;
                        }
                        async_accept(); // start waiting for a new connection
                        onConnection(lpOverlapped->Socket_);
                        break;
                    case IO_OPERATION::IoRead:
                        lpOverlapped->transfered_bytes += numberofbytestransfered;
                        lpOverlapped->Socket_->continue_read(lpOverlapped);
                        break;
                    case IO_OPERATION::IoWrite:
                        lpOverlapped->transfered_bytes += numberofbytestransfered;
                        lpOverlapped->Socket_->continue_write(lpOverlapped);
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