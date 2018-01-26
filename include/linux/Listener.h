#pragma once
#include "Socket_Lite.h"
#include "Structures.h"
#include <memory>
namespace SL
{
namespace NET
{
class Socket;
class Listener final : public IListener
{
    std::shared_ptr<Socket> ListenSocket;
    Win_IO_Accept_Context Win_IO_Accept_Context_;
    std::shared_ptr<IO_Context> IO_Context_;
    sockaddr ListenSocketAddr;
    IOCP iocp;
    std::thread AcceptThread;
public:


    Listener(const std::shared_ptr<IO_Context>& context, std::shared_ptr<ISocket> &&socket, const sockaddr& addr);
    virtual ~Listener();
    virtual void close() override;
    virtual void async_accept(const std::function<void(bool)> &&handler)override;
    void handleaccept(epoll_event% ev);

};

} // namespace NET
} // namespace SL
