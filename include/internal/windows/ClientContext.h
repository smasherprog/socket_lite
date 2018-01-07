#pragma once

#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
namespace SL {
namespace NET {

    class ClientContext : public IClientContext {
      public:
        WSARAII wsa;
        IOCP iocp;

        std::function<void(const std::shared_ptr<ISocket> &)> onConnection;
        std::function<void(const std::shared_ptr<ISocket> &)> onDisconnection;

        bool KeepRunning = true;
        std::vector<std::thread> Threads;
        std::shared_ptr<Socket> Socket_;

        ClientContext();
        virtual ~ClientContext();
        virtual void run(ThreadCount threadcount) override;
        virtual bool async_connect(std::string host, PortNumber port) override;

        virtual void setonConnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) override { onConnection = handle; }
        virtual void setonDisconnection(const std::function<void(const std::shared_ptr<ISocket> &)> &handle) override { onDisconnection = handle; }
    };

} // namespace NET
} // namespace SL