#pragma once
#include "Context.h"
namespace SL {
namespace NET {
    class IO_Service {
        bool keepgoing = true;

      public:
        IO_Service(Context &context) {}
        ~IO_Service() { keepgoing = false; }

        void run()
        {
            while (true) {
                DWORD dwIoSize = 0;
                LPWSAOVERLAPPED lpOverlapped = nullptr;
                SocketContext *lpPerSocketContext = nullptr;
                auto gotevents =
                    GetQueuedCompletionStatus(iocp.handle, &dwIoSize, (PDWORD_PTR)&lpPerSocketContext, (LPOVERLAPPED *)&lpOverlapped, INFINITE);
                if (!keepgoing) {
                    return;
                }
            }
        }
        friend class Acceptor;
    };
} // namespace NET
} // namespace SL