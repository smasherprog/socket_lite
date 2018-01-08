#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include "internal/SpinLock.h"
#include <functional>
#include <memory>
namespace SL {
namespace NET {
    class Socket final : public ISocket, public Node<Socket> {
      public:
        SpinLock WriteContextsLock;
        Win_IO_Context_List *WriteContexts = nullptr;
        std::unique_ptr<Win_IO_Context> ReadContext;

        Socket();
        virtual ~Socket();

        virtual void async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler);
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler);
        virtual void close();

        void read_complete(Bytes_Transfered b, Win_IO_Context *sockcontext);
        Win_IO_Context_List *write_complete(Bytes_Transfered b, Win_IO_Context_List *sockcontext);
        void continue_read(Win_IO_Context *sockcontext);
        void continue_write(Win_IO_Context_List *sockcontext);
    };

} // namespace NET
} // namespace SL
