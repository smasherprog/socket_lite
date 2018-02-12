#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <memory>
#include <vector>
#include <thread>

namespace SL
{
namespace NET
{
class Socket;
class Context;
class Listener final : public IListener
{
    Context* Context_;
    std::shared_ptr<Socket> ListenSocket;
    Win_IO_Accept_Context Win_IO_Accept_Context_;
    sockaddr ListenSocketAddr;

public:


    Listener(Context* context, std::shared_ptr<ISocket> &&socket, const sockaddr& addr);
    virtual ~Listener();
    virtual void close() override;
    virtual void async_accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket>&)> &&handler)override;
    void handleaccept(epoll_event& ev);

};

} // namespace NET
} // namespace SL
