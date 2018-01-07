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

        bool KeepRunning = true;
        std::vector<std::thread> Threads;
        std::shared_ptr<Socket> Socket_;

        ClientContext();
        virtual ~ClientContext();
        virtual void run(ThreadCount threadcount) override;
        virtual bool async_connect(std::string host, PortNumber port) override;
        void closeclient(IO_OPERATION op, PER_IO_CONTEXT *context);
    };

} // namespace NET
} // namespace SL