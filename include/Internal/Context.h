#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

namespace SL {
namespace NET {
    class Context final : public IContext {
        std::vector<std::thread> Threads;
        std::vector<SOCKET> ConnectionSockets;
        std::mutex ConnectionSocketsLock;

      public:
#if WIN32
        WSARAII wsa;
        SOCKET getSocket(AddressFamily family);
        void handle_buildconnectionsocket(bool success, Win_IO_BuildConnectSocket_Context *context);
#else
        int EventWakeFd = -1;
#endif

        IOCP iocp;
        std::atomic<int> PendingIO;

        Context();
        ~Context();
        virtual void run(ThreadCount threadcount) override;
        virtual std::shared_ptr<ISocket> CreateSocket() override;
        virtual std::shared_ptr<IListener> CreateListener(std::shared_ptr<ISocket> &&listensocket) override;
        bool inWorkerThread() const
        {
            return std::any_of(std::begin(Threads), std::end(Threads), [](const std::thread &t) { return t.get_id() == std::this_thread::get_id(); });
        }
    };
} // namespace NET
} // namespace SL
