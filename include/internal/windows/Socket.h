#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include "internal/SpinLock.h"
#include <functional>

namespace SL {
namespace NET {
    class Socket final : public ISocket {
      public:
        SpinLock Lock;
        std::list<PER_IO_CONTEXT *> SendBuffers;
        PER_IO_CONTEXT *ReadContext = nullptr;

        Socket();
        virtual ~Socket();

        virtual void async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler);
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler);
        virtual void close();

        void read_complete(Bytes_Transfered b, PER_IO_CONTEXT *sockcontext);
        PER_IO_CONTEXT *write_complete(Bytes_Transfered b, PER_IO_CONTEXT *sockcontext);
        void continue_read(PER_IO_CONTEXT *sockcontext);
        void continue_write(PER_IO_CONTEXT *sockcontext);
    };
} // namespace NET
} // namespace SL
