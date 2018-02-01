#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <thread>

namespace SL {
namespace NET {
    class Listener;
    class IO_Context final : public IIO_Context {

        bool KeepRunning = true;
        std::vector<std::thread> Threads;

      public:
        WSARAII wsa;
        IOCP iocp;
        std::atomic<size_t> PendingIO;
        LPFN_CONNECTEX ConnectEx_ = nullptr;

        IO_Context();
        ~IO_Context();
        virtual void run(ThreadCount threadcount) override;
        virtual std::shared_ptr<ISocket> CreateSocket() override;

        void handleaccept(bool success, Win_IO_Accept_Context *overlapped);
        void handleconnect(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped);
        void handlerecv(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped, DWORD trasnferedbytes);
        void handlewrite(bool success, Socket *completionkey, Win_IO_RW_Context *overlapped, DWORD trasnferedbytes);
    };
} // namespace NET
} // namespace SL