#pragma once
#include "Socket_Lite.h"
#include "common/SpinLock.h"
#include "common/Structures.h"
#include <functional>
#include <memory>
namespace SL {
namespace NET {

    class Socket final : public ISocket {

      public:
        SpinLock WriteContextsLock;
        Win_IO_Context_List *WriteContexts = nullptr;
        Win_IO_Context ReadContext;

        Socket();
        virtual ~Socket();

        virtual void async_connect(const std::shared_ptr<IIO_Context> &io_context, sockaddr addr,
                                   const std::function<void(Bytes_Transfered)> &&handler);
        virtual void async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler);
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler);
        // send a close message and close the socket
        virtual void close();
        SOCKET get_handle() const { return handle; }
    };

} // namespace NET
} // namespace SL
