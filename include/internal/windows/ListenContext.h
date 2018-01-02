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
        std::vector<std::thread> Threads;
        Acceptor Acceptor;
        bool KeepRunning = true;
        IOCP iocp;

        ListenContext(PortNumber port, NetworkProtocol protocol);
        virtual ~ListenContext();
        void run(ThreadCount threadcount);
        virtual void set_MaxPayload(size_t bytes);
        virtual size_t get_MaxPayload();
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