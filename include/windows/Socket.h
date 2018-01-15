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

        Win_IO_RW_Context ReadContext;
        Win_IO_RW_Context WriteContext;
        std::weak_ptr<IO_Context> IO_Context_;

        Socket(std::shared_ptr<IO_Context> &context, Address_Family family);
        virtual ~Socket();
        static bool UpdateIOCP(SOCKET socket, HANDLE *iocp, void *completionkey);
        virtual void connect(SL::NET::sockaddr &address, const std::function<void(ConnectionAttemptStatus)> &&) override;
        virtual void recv(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) override;
        virtual void async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) override;
        // send a close message and close the socket
        virtual void close() override;
        SOCKET get_handle() const { return handle; }
        void continue_write(Win_IO_RW_Context *sockcontext);
        void continue_read(Win_IO_RW_Context *sockcontext);
        void continue_connect(ConnectionAttemptStatus connect_success, Win_IO_Connect_Context *sockcontext);
        void continue_connect(Win_IO_Connect_Context *sockcontext);
    };

} // namespace NET
} // namespace SL
