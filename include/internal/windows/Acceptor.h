#pragma once

#include "Socket_Lite.h"
#include "Structures.h"
#include <memory>
namespace SL {
namespace NET {
    class Acceptor {
      public:
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        HANDLE *IOCPHandle = NULL;
        SOCKET AcceptSocket = INVALID_SOCKET;
        NetworkProtocol Protocol;
        unsigned char AcceptBuffer[2 * (sizeof(SOCKADDR_STORAGE) + 16)];
        Acceptor(HANDLE *iocphandle, PortNumber port, NetworkProtocol protocol);
        ~Acceptor();

        void async_accept();
    };
} // namespace NET
} // namespace SL