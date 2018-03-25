#pragma once
#include "Socket_Lite.h"
#include "defs.h"
#include <algorithm>
#include <thread>
#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <mswsock.h>
#endif

namespace SL {
namespace NET {

    class Context final : public IContext {
        std::vector<std::thread> Threads;
        ThreadCount ThreadCount_;

      public:
#if WIN32
        WSADATA wsaData;
        LPFN_CONNECTEX ConnectEx_ = nullptr;
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        HANDLE IOCPHandle = NULL;
#else
        int EventWakeFd = -1;
        int IOCPHandle = -1;
#endif

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
