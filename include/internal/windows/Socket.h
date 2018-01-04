#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <vector>

namespace SL {
namespace NET {
    class FastAllocator;
    class Context;
    class Socket final : public ISocket {
      public:
        static SOCKET Create(NetworkProtocol protocol);
        SOCKET handle = INVALID_SOCKET;
        Context *Context_ = nullptr;
        PER_IO_CONTEXT *PER_IO_CONTEXT_ = nullptr;

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

        bool read(FastAllocator &allocator);
        bool send();
    };
} // namespace NET
} // namespace SL
