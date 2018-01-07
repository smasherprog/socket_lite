#include "ClientContext.h"
#include "Socket.h"
namespace SL {
namespace NET {

    ClientContext::ClientContext() {}
    ClientContext::~ClientContext()
    {
        KeepRunning = false;

        for (auto &t : Threads) {
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
    bool ClientContext::async_connect(std::string host, PortNumber port)
    {
        Socket_.reset();
        auto socket = std::make_shared<Socket>(protocol);
        if (protocol == NetworkProtocol::IPV4) {
            SOCKADDR_IN SockAddr = {0};
            SockAddr.sin_family = AF_INET;
            SockAddr.sin_addr.s_addr = inet_addr(host.c_str());
            SockAddr.sin_port = htons(port);

            if (WSAConnect(socket->handle, (SOCKADDR *)(&SockAddr), sizeof(SockAddr), NULL, NULL, NULL, NULL) == SOCKET_ERROR) {
                return onDisconnection(socket);
            }
        }
        else {
            sockaddr_in6 SockAddr = {0};
            SockAddr.sin6_family = AF_INET6;
            inet_pton(AF_INET6, host.c_str(), &(SockAddr.sin6_addr));
            SockAddr.sin6_port = htons(port);

            if (WSAConnect(socket->handle, (SOCKADDR *)(&SockAddr), sizeof(SockAddr), NULL, NULL, NULL, NULL) == SOCKET_ERROR) {
                return onDisconnection(socket);
            }
        }
        if (auto ret = setsockopt(socket->handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0); ret == SOCKET_ERROR) {
            onDisconnection(socket);
        }
        else {
            Socket_ = socket;
        }
    }
    void ClientContext::run(ThreadCount threadcount)
    {

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
                        continue;
                    }

                    switch (lpOverlapped->IOOperation) {
                    case IO_OPERATION::IoAccept:
                        if (auto ret = setsockopt(lpOverlapped->Socket_->handle, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                                                  (char *)&Acceptor_.AcceptSocket, sizeof(Acceptor_.AcceptSocket));
                            ret == SOCKET_ERROR) {
                            // big error here...
                            return;
                        }
                        if (!updateIOCP(lpOverlapped->Socket_->handle, &iocp.handle)) {
                            // big error here...
                            return;
                        }
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