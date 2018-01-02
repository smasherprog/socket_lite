#pragma once

#include "Socket_Lite.h"
#include "Structures.h"
#include <memory>
namespace SL {
namespace NET {
    class ListenContext;
    class ISocket;
    class Acceptor {
        LPFN_ACCEPTEX AcceptEx_ = nullptr;
        ListenContext &Context_;
        PER_IO_CONTEXT AcceptContext = {0};
        SOCKET AcceptSocket = INVALID_SOCKET;
        NetworkProtocol Protocol;
        unsigned char AcceptBuffer[2 * (sizeof(SOCKADDR_STORAGE) + 16)];

      public:
        Acceptor(ListenContext &context, PortNumber port, NetworkProtocol protocol);
        ~Acceptor();

        void async_accept();
    };
} // namespace NET
} // namespace SL