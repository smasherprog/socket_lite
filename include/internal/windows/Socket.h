#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <list>

namespace SL {
namespace NET {
    class FastAllocator;
    class Context;
    class Socket final : public ISocket {
      public:
        static SOCKET Create(NetworkProtocol protocol);
        SOCKET handle = INVALID_SOCKET;
        Context *Parent = nullptr;
        SocketStatus SocketStatus_ = SocketStatus::CLOSED;

        std::mutex SendBuffersLock;
        std::list<PER_IO_CONTEXT> SendBuffers;
        std::unique_ptr<PER_IO_CONTEXT> ReadContext;

        Socket(NetworkProtocol protocol);
        virtual ~Socket();
        virtual SocketStatus get_status() const;
        virtual std::string get_address() const;
        virtual unsigned short get_port() const;
        virtual NetworkProtocol get_protocol() const;
        virtual bool is_loopback() const;
        virtual void async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(size_t)> &handler);
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(size_t)> &handler);
        virtual void close();

        SocketStatus continue_read(PER_IO_CONTEXT *sockcontext);
        SocketStatus continue_write(PER_IO_CONTEXT *sockcontext);
    };
} // namespace NET
} // namespace SL
