#include "Acceptor.h"
#include "ClientContext.h"
namespace SL {
namespace NET {

    ClientContext::ClientContext(PortNumber port, NetworkProtocol protocol) {}
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

    void ClientContext::set_MaxPayload(size_t bytes) {}

    size_t ClientContext::get_MaxPayload() { return size_t(); }

    void ClientContext::set_ReadTimeout(std::chrono::seconds seconds) {}

    std::chrono::seconds ClientContext::get_ReadTimeout() { return std::chrono::seconds(); }

    void ClientContext::set_WriteTimeout(std::chrono::seconds seconds) {}

    std::chrono::seconds ClientContext::get_WriteTimeout() { return std::chrono::seconds(); }

} // namespace NET
} // namespace SL