#pragma once
#include "Acceptor.h"
#include "Context.h"
#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
namespace SL {
namespace NET {

    class ListenContext : public Context {
      public:
        bool KeepRunning = true;
        std::vector<std::thread> Threads;
        Acceptor Acceptor_;
        NetworkProtocol Protocol;

        ListenContext(PortNumber port, NetworkProtocol protocol);
        virtual ~ListenContext();
        void run(ThreadCount threadcount);
    };

} // namespace NET
} // namespace SL