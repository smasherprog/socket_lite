#include "Acceptor.h"
#include "ListenContext.h"
#include "Socket.h"
namespace SL {
namespace NET {

    ListenContext::ListenContext(PortNumber port, NetworkProtocol protocol) : Acceptor_(&iocp.handle, port, protocol) {}
    ListenContext::~ListenContext()
    {
        KeepRunning = false;

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

    void ListenContext::run(ThreadCount threadcount)
    {
        Threads.reserve(threadcount.value);
        for (auto i = 0; i < threadcount.value; i++) {
            Threads.push_back(std::thread([&] {
                while (KeepRunning) {
                    DWORD dwIoSize = 0;
                    PER_IO_CONTEXT *lpOverlapped = nullptr;
                    void *lpPerSocketContext = nullptr;
                    auto bSuccess =
                        GetQueuedCompletionStatus(iocp.handle, &dwIoSize, (PDWORD_PTR)&lpPerSocketContext, (LPOVERLAPPED *)&lpOverlapped, 100);
                    if (!KeepRunning) {
                        // get out this thread is done meow!
                        return;
                    }
                    if (bSuccess == FALSE && lpOverlapped == NULL) {
                        continue; // timer ran out go back to top and try again
                    }
                    if (lpOverlapped->IOOperation != IO_OPERATION::IoAccept && (bSuccess == FALSE || (bSuccess == TRUE && 0 == dwIoSize))) {
                        // dropped connection
                        onDisconnection(lpOverlapped->Socket);
                        freecontext(&lpOverlapped);
                        continue;
                    }
                    DWORD flag(0), recvbyte(0);
                    switch (lpOverlapped->IOOperation) {
                    case IO_OPERATION::IoAccept:
                        if (auto ret = setsockopt(lpOverlapped->Socket->handle, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&Acceptor_.AcceptSocket,
                                                  sizeof(Acceptor_.AcceptSocket));
                            ret == SOCKET_ERROR) {
                            // big error here...
                            return;
                        }
                        if (!updateIOCP(lpOverlapped->Socket->handle, &iocp.handle)) {
                            // big error here...
                            return;
                        }
                        Acceptor_.async_accept(); // this will start another accept

                        // i chose to use the read zero bytes cheat here. google it...   zero byte WSARecv
                        if (auto ret =
                                WSARecv(lpOverlapped->Socket->handle, &lpOverlapped->wsabuf, 1, &recvbyte, &flag, &lpOverlapped->Overlapped, NULL);
                            ret == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError()) {
                            // not good close down socket
                            onDisconnection(lpOverlapped->Socket);
                            freecontext(&lpOverlapped);
                        }

                        break;
                    case IO_OPERATION::IoRead:

                        break;
                    case IO_OPERATION::IoWrite:

                        break;
                    default:
                        break;
                    }
                }
            }));
        }
    }

    void ListenContext::set_ReadTimeout(std::chrono::seconds seconds) {}

    std::chrono::seconds ListenContext::get_ReadTimeout() { return std::chrono::seconds(); }

    void ListenContext::set_WriteTimeout(std::chrono::seconds seconds) {}

    std::chrono::seconds ListenContext::get_WriteTimeout() { return std::chrono::seconds(); }

} // namespace NET
} // namespace SL