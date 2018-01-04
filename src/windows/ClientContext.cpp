#include "Acceptor.h"
#include "ClientContext.h"
namespace SL {
namespace NET {

    ClientContext::ClientContext(std::string host, PortNumber port, NetworkProtocol protocol) {}
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

    void ClientContext::run(ThreadCount threadcount)
    {
        Threads.reserve(threadcount.value);
        for (auto i = 0; i < threadcount.value; i++) {
            Threads.push_back(std::thread([&] {

            }));
        }
    }

} // namespace NET
} // namespace SL