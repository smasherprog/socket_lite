#include "Acceptor.h"
#include "ListenContext.h"
#include "Socket.h"
#include "internal/FastAllocator.h"
namespace SL {
namespace NET {

    ListenContext::ListenContext(PortNumber port, NetworkProtocol protocol) : Acceptor_(&iocp.handle, port, protocol), Protocol(protocol) {}
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
        Acceptor_.async_accept(); // start waiting for a new connection
        Threads.reserve(threadcount.value);
        for (auto i = 0; i < threadcount.value; i++) {
            Threads.push_back(std::thread([&] {
                FastAllocator allocator;
                allocator.capacity(1024 * 1024); // 1 MB
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
                        Acceptor_.async_accept(); // start waiting for a new connection

                        lpOverlapped->Socket_->SocketStatus_ = SocketStatus::CONNECTED;
                        onConnection(lpOverlapped->Socket_);
                        break;
                    case IO_OPERATION::IoRead:
                        lpOverlapped->transfered_bytes += numberofbytestransfered;
                        lpOverlapped->Socket->continue_read(lpOverlapped);
                        break;
                    case IO_OPERATION::IoWrite:
                        lpOverlapped->transfered_bytes += numberofbytestransfered;
                        lpOverlapped->Socket->continue_write(lpOverlapped);
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