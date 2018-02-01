#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace SL {
namespace NET {

    class Socket final : public ISocket {
      public:
        Context *Context_;
        Win_IO_RW_Context ReadContext;
        Win_IO_RW_Context WriteContext;

        Socket(Context *context);
        Socket(Context *context, Address_Family family);
        virtual ~Socket();
        static bool UpdateIOCP(SOCKET socket, HANDLE *iocp, void *completionkey);
        virtual void connect(sockaddr &address, const std::function<void(ConnectionAttemptStatus)> &&) override;
        virtual void recv(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) override;
        virtual void send(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler) override;

        void continue_write(bool success, Win_IO_RW_Context *sockcontext);
        void continue_read(bool success, Win_IO_RW_Context *sockcontext);
        void continue_connect(ConnectionAttemptStatus connect_success, Win_IO_RW_Context *sockcontext);
    };

} // namespace NET
} // namespace SL
