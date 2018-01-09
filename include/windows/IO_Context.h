#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <thread>
namespace SL {
namespace NET {

    class IO_Context final : public IIO_Context {

        bool KeepRunning = true;
        std::vector<std::thread> Threads;

      public:
        WSARAII wsa;
        IOCP iocp;
        IO_Context();
        ~IO_Context();
        void run(ThreadCount threadcount);
    };

} // namespace NET
} // namespace SL