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
    class Socket;
    struct Win_IO_Context;
    class ClientContext : public IClientContext {
      public:
        WSARAII wsa;
        IOCP iocp;
        Win_IO_Context ConnectIOContext;
        bool KeepRunning = true;
        std::vector<std::thread> Threads;
        std::shared_ptr<Socket> Socket_;

        ClientContext();
        virtual ~ClientContext();
        virtual void run(ThreadCount threadcount) override;
        virtual bool async_connect(std::string host, PortNumber port) override;
        void closeclient(IO_OPERATION op, Win_IO_Context *context);
    };

} // namespace NET
} // namespace SL