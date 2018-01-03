#pragma once
#include "Acceptor.h"
#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
namespace SL {
namespace NET {

    class ListenContext : public IContext {
      public:
        WSARAII wsa;
        IOCP iocp;
        bool KeepRunning = true;
        std::vector<std::thread> Threads;
        Acceptor Acceptor_;

        ListenContext(PortNumber port, NetworkProtocol protocol);
        virtual ~ListenContext();
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