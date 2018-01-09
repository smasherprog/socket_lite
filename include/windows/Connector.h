#pragma once

#include "Socket_Lite.h"
#include "Structures.h"

namespace SL {
namespace NET {
    class Connector {
      public:
        HANDLE *IOCPHandle = NULL;
        NetworkProtocol Protocol;
        std::string Host;
        PortNumber Port;
        Connector(HANDLE *iocphandle, std::string host, PortNumber port, NetworkProtocol protocol);
        ~Connector();

        void async_connect();
    };
} // namespace NET
} // namespace SL