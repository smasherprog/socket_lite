#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <functional>
#include <memory>

namespace SL {
namespace NET {

    class Socket final : public ISocket {
      public:
        Context *Context_;
        Socket(Context *context);
        Socket(Context *context, AddressFamily family);
        virtual ~Socket();
        static bool UpdateIOCP(SOCKET socket, HANDLE *iocp, void *completionkey);
        virtual void connect(sockaddr &address, const std::function<void(StatusCode)> &&) override;
        virtual void recv(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler) override;
        virtual void send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler) override;

        void continue_io(bool success, Win_IO_RW_Context *context);
        void init_connect(bool success, Win_IO_Connect_Context *context);
        void continue_connect(bool success, Win_IO_Connect_Context *context);
    };

} // namespace NET
} // namespace SL
