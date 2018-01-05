#pragma once

#include "Context.h"
#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
namespace SL {
namespace NET {

    class ClientContext : public Context {
      public:
        bool KeepRunning = true;
        std::vector<std::thread> Threads;
        NetworkProtocol Protocol;

        ClientContext(std::string host, PortNumber port, NetworkProtocol protocol);
        virtual ~ClientContext();
        void run(ThreadCount threadcount);
    };

} // namespace NET
} // namespace SL