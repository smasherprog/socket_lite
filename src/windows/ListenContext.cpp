#include "Acceptor.h"
#include "ListenContext.h"
#include "Socket.h"
namespace SL {
namespace NET {

    ListenContext::ListenContext(PortNumber port, NetworkProtocol protocol) : Acceptor(*this, port, protocol) {}
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
                        auto ptr = lpOverlapped->Socket;
                        onDisconnection(ptr);
                        freecontext(&lpOverlapped);
                        continue;
                    }
                }
            }));
        }
    }

    void ListenContext::set_MaxPayload(size_t bytes) {}

    size_t ListenContext::get_MaxPayload() { return size_t(); }

    void ListenContext::set_ReadTimeout(std::chrono::seconds seconds) {}

    std::chrono::seconds ListenContext::get_ReadTimeout() { return std::chrono::seconds(); }

    void ListenContext::set_WriteTimeout(std::chrono::seconds seconds) {}

    std::chrono::seconds ListenContext::get_WriteTimeout() { return std::chrono::seconds(); }

} // namespace NET
} // namespace SL