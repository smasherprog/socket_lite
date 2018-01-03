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

    class ClientContext : public IContext {
      public:
        WSARAII wsa;
        std::vector<std::thread> Threads;

        bool KeepRunning = true;
        IOCP iocp;

        ClientContext(std::string host, PortNumber port, NetworkProtocol protocol);
        virtual ~ClientContext();
        void run(ThreadCount threadcount);
        virtual void set_ReadTimeout(std::chrono::seconds seconds);
        virtual std::chrono::seconds get_ReadTimeout();
        virtual void set_WriteTimeout(std::chrono::seconds seconds);
        virtual std::chrono::seconds get_WriteTimeout();

        std::function<void(const std::shared_ptr<ISocket> &)> onConnection;
        std::function<void(const std::shared_ptr<ISocket> &, const Message &)> onMessage;
        std::function<void(const std::shared_ptr<ISocket> &)> onDisconnection;
    };

} // namespace NET
} // namespace SL