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
#define MAX_BUFF_SIZE 8192
    class Socket;
    class ListenContext final : public IListenContext {
      public:
        WSARAII wsa;
        IOCP iocp;

        bool KeepRunning = true;
        std::vector<std::thread> Threads;
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        char Buffer[MAX_BUFF_SIZE];
        SOCKET ListenSocket = INVALID_SOCKET;
        Win_IO_Context ListenIOContext;
        int AddressFamily = AF_INET;

        SpinLock ClientLock;
        Socket *Clients = nullptr;

        ListenContext();
        virtual ~ListenContext();
        virtual bool bind(PortNumber port) override;
        virtual bool listen() override;
        virtual void run(ThreadCount threadcount) override;

        void closeclient(Socket *socket, Win_IO_Context *context);
        void async_accept();
    };

} // namespace NET
} // namespace SL