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
        std::shared_ptr<Listener> Listener_;
        std::atomic<size_t> PendingIO;
        IO_Context();
        ~IO_Context();
        void run(ThreadCount threadcount);

        void handleaccept(WinIOContext *overlapped);
        void handleconnect(bool success, Socket *completionkey, WinIOContext *overlapped);
        void handlerecv(bool success, Socket *completionkey, WinIOContext *overlapped, DWORD trasnferedbytes);
        void handlewrite(bool success, Socket *completionkey, WinIOContext *overlapped, DWORD trasnferedbytes);
    };

} // namespace NET
} // namespace SL