#pragma once
#include "Socket_Lite.h"
#include "defs.h"
#include <functional>
#include <memory>

namespace SL
{
namespace NET
{

class Socket final : public ISocket
{
public:
    Context *Context_;
#ifndef _WIN32
    Win_IO_RW_Context ReadContext;
    Win_IO_RW_Context WriteContext;
#endif
    Socket(Context *context);
    Socket(Context *context, AddressFamily family);
    virtual ~Socket();
    virtual void connect(sockaddr &address, const std::function<void(StatusCode)> &&) override;
    virtual void recv(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler) override;
    virtual void send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler) override;

    static void continue_io(bool success, Win_IO_RW_Context *context);
#if _WIN32
    static void init_connect(bool success, Win_IO_Connect_Context *context);
    static void continue_connect(bool success, Win_IO_Connect_Context *context);
#else
    virtual void close() override;
    static void continue_connect(bool success, Win_IO_RW_Context *context);
#endif
};

} // namespace NET
} // namespace SL
