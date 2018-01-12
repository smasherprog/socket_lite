#pragma once
#include "Socket_Lite.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
namespace SL {
namespace NET {
    enum class SocketIO_Status { StillTransfering, DoneTransfering };
    class Socket final : public ISocket {
      public:
        std::atomic<size_t> &PendingIO;

        Socket(std::atomic<size_t> &counter);
        virtual ~Socket();
        static bool UpdateIOCP(SOCKET socket, HANDLE *iocp, void *completionkey);
        virtual void async_connect(const std::shared_ptr<IIO_Context> &io_context, std::vector<SL::NET::sockaddr> &addresses,
                                   const std::function<bool(bool, SL::NET::sockaddr &)> &&) override;
        virtual void async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) override;
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) override;
        // send a close message and close the socket
        virtual void close() override;
        SOCKET get_handle() const { return handle; }
        void continue_write(Win_IO_RW_Context *sockcontext);
        void continue_read(Win_IO_RW_Context *sockcontext);
        void continue_connect(bool connect_success, Win_IO_Connect_Context *sockcontext);
    };

} // namespace NET
} // namespace SL
