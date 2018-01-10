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
        static bool UpdateIOCP(SOCKET socket, HANDLE *iocp, void *completionkey);
        virtual void async_connect(const std::shared_ptr<IIO_Context> &io_context, sockaddr addr,
                                   const std::function<void(Bytes_Transfered)> &&handler) override;
        virtual void async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) override;
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) override;
        // send a close message and close the socket
        virtual void close() override;
        SOCKET get_handle() const { return handle; }
    };

} // namespace NET
} // namespace SL
