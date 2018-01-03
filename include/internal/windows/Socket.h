#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
namespace SL {
namespace NET {
    class Socket final : public ISocket {
      public:
        static SOCKET Create(NetworkProtocol protocol);
        SOCKET handle = INVALID_SOCKET;

        Socket(NetworkProtocol protocol);
        virtual ~Socket();
        virtual SocketStatus is_open() const;
        virtual std::string get_address() const;
        virtual unsigned short get_port() const;
        virtual NetworkProtocol get_protocol() const;
        virtual bool is_loopback() const;
        virtual size_t BufferedBytes() const;
        virtual void send(const Message &msg);
        virtual void close();
    };
} // namespace NET
} // namespace SL
