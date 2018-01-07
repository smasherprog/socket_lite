#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include "internal/SpinLock.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <unordered_set>
namespace SL {
namespace NET {

    class ListenContext final : public IListenContext {
      public:
        WSARAII wsa;
        IOCP iocp;

        bool KeepRunning = true;
        std::vector<std::thread> Threads;
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        unsigned char AcceptBuffer[2 * (sizeof(SOCKADDR_STORAGE) + 16)];
        SOCKET ListenSocket = INVALID_SOCKET;
        PER_IO_CONTEXT ListenIOContext;
        int AddressFamily = AF_INET;

        SpinLock ClientLock;
        std::unordered_set<std::shared_ptr<Socket>> Clients;

        ListenContext();
        virtual ~ListenContext();
        virtual bool bind(PortNumber port) override;
        virtual bool listen() override;
        virtual void run(ThreadCount threadcount) override;

        void closeclient(Socket *socket, IO_OPERATION op, PER_IO_CONTEXT *context);
        void closeclient(const std::shared_ptr<Socket> &socket, IO_OPERATION op, PER_IO_CONTEXT *context);
        void async_accept();
    };

} // namespace NET
} // namespace SL