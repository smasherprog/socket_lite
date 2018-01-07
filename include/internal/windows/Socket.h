#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <functional>
#include <list>

namespace SL {
namespace NET {
    class Socket final : public ISocket {
      public:
        std::mutex SendBuffersLock;
        std::list<PER_IO_CONTEXT> SendBuffers;
        std::unique_ptr<PER_IO_CONTEXT> ReadContext;

        Socket();
        virtual ~Socket();

        virtual void async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &handler);
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &handler);
        virtual void close();

        void continue_read(PER_IO_CONTEXT *sockcontext);
        void continue_write(PER_IO_CONTEXT *sockcontext);
    };
} // namespace NET
} // namespace SL
