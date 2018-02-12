#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace SL
{
namespace NET
{
class Context;
class Socket final : public ISocket
{
public:
    Context* Context_;
    Win_IO_RW_Context ReadContext;
    Win_IO_RW_Context WriteContext;

    Socket(Context* context);
    Socket(Context* context, AddressFamily family);
    virtual ~Socket();
    virtual void connect(SL::NET::sockaddr &address, const std::function<void(StatusCode)> &&) override;
    virtual void recv(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler) override;
    virtual void send(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler) override;

    void handlerecv(bool success, Win_IO_RW_Context* context);
    void handlewrite(bool success, Win_IO_RW_Context* context);
};

} // namespace NET
} // namespace SL
