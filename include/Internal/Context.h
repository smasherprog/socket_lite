#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include "memorypool.h"
#include <algorithm>
#include <thread>
namespace SL {
namespace NET {
    class Context final : public IContext {
        std::vector<std::thread> Threads;

      public:
#if WIN32
        WSARAII wsa;
#else
        int EventWakeFd = -1;
#endif

        IOCP iocp;
        std::atomic<int> PendingIO;
        MemoryPool<Win_IO_RW_Context> RW_ContextPool;
        MemoryPool<RW_CompletionHandler> RW_CompletionHandlerPool;

        Context();
        ~Context();
        virtual void run(ThreadCount threadcount) override;
        virtual std::shared_ptr<ISocket> CreateSocket() override;
        virtual std::shared_ptr<IListener> CreateListener(std::shared_ptr<ISocket> &&listensocket) override;
        bool inWorkerThread() const
        {
            return std::any_of(std::begin(Threads), std::end(Threads), [](const std::thread &t) { return t.get_id() == std::this_thread::get_id(); });
        }
    };
} // namespace NET
} // namespace SL
