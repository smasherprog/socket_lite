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
    IOCP iocp;
    std::thread AcceptThread;
public:


    Listener(std::atomic<size_t> &counter, std::shared_ptr<ISocket> &&socket);
    virtual ~Listener();
    virtual void close();
    virtual void async_accept(std::shared_ptr<ISocket> &socket, const std::function<void(bool)> &&handler);
    void handleaccept(epoll_event% ev);

};

} // namespace NET
} // namespace SL
