#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
namespace SL {
namespace NET {

    class ListenContext final : public IListenContext {
      public:
        WSARAII wsa;
        IOCP iocp;

        std::function<void(const std::shared_ptr<ISocket> &)> onConnection;
        std::function<void(const std::shared_ptr<ISocket> &)> onDisconnection;

        bool KeepRunning = true;
        std::vector<std::thread> Threads;
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        unsigned char AcceptBuffer[2 * (sizeof(SOCKADDR_STORAGE) + 16)];
        SOCKET ListenSocket = INVALID_SOCKET;

        ListenContext();
        virtual ~ListenContext();
        virtual bool bind(PortNumber port) override;
        virtual bool listen() override;
        virtual void run(ThreadCount threadcount) override;
        virtual void setonConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) override { onConnection = handle; }
        virtual void setonDisconnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) override { onDisconnection = handle; }

        void async_accept();
    };

} // namespace NET
} // namespace SL