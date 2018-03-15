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
        ThreadCount ThreadCount_;

      public:
        MemoryPool<Win_IO_RW_Context *, Win_IO_RW_ContextCRUD> Win_IO_RW_ContextBuffer;
        MemoryPool<RW_CompletionHandler *, RW_CompletionHandlerCRUD> RW_CompletionHandlerBuffer;

#if WIN32
        WSARAII wsa;
#else
        int EventWakeFd = -1;
#endif

        IOCP iocp;
        std::atomic<int> PendingIO;

        Context(ThreadCount threadcount);
        ~Context();
        virtual void run() override;
        virtual std::shared_ptr<ISocket> CreateSocket() override;
        virtual std::shared_ptr<IListener> CreateListener(std::shared_ptr<ISocket> &&listensocket) override;
        bool inWorkerThread() const
        {
            return std::any_of(std::begin(Threads), std::end(Threads), [](const std::thread &t) { return t.get_id() == std::this_thread::get_id(); });
        }
    };
} // namespace NET
} // namespace SL
