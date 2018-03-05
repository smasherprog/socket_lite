#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <thread>

namespace SL {
namespace NET {
    class Context final : public IContext {

        std::vector<std::thread> Threads;

      public:
#if WIN32
        WSARAII wsa;
        LPFN_CONNECTEX ConnectEx_ = nullptr;
#endif
        IOCP iocp;
        std::atomic<int> PendingIO;

        Context();
        ~Context();
        virtual void run(ThreadCount threadcount) override;
        virtual std::shared_ptr<ISocket> CreateSocket() override;
        virtual std::shared_ptr<IListener> CreateListener(std::shared_ptr<ISocket> &&listensocket) override;
    };
} // namespace NET
} // namespace SL