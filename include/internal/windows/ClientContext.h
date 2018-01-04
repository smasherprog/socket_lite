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

        std::function<void(const std::shared_ptr<ISocket> &)> onConnection;
        std::function<void(const std::shared_ptr<ISocket> &, const unsigned char *data, size_t len)> onData;
        std::function<void(const std::shared_ptr<ISocket> &)> onDisconnection;
    };

} // namespace NET
} // namespace SL