#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace SL {
namespace NET {

    extern LPFN_CONNECTEX ConnectEx_;
    class Socket final : public ISocket {
      public:
        Context *Context_;
        Win_IO_RW_Context ReadContext;
        Win_IO_RW_Context WriteContext;

        Socket(Context *context);
        Socket(Context *context, AddressFamily family);
        virtual ~Socket();
        static bool UpdateIOCP(SOCKET socket, HANDLE *iocp, void *completionkey);
        virtual void connect(sockaddr &address, const std::function<void(StatusCode)> &&) override;
        virtual void recv(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler) override;
        virtual void send(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler) override;

        void increment_writebytes(DWORD bytes) { WriteContext.transfered_bytes += bytes; }
        void increment_readbytes(DWORD bytes) { ReadContext.transfered_bytes += bytes; }

        void continue_write(bool success);
        void continue_read(bool success);
        void init_connect(bool success, Win_IO_Connect_Context *context);
        void continue_connect(bool success, Win_IO_Connect_Context *context);
    };

} // namespace NET
} // namespace SL
