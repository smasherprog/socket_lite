#include "Listener.h"
#include "Socket.h"
#include <assert.h>
#include <iostream>

namespace SL
{
namespace NET
{

Listener::Listener( Context* context,std::shared_ptr<ISocket> &&socket, const sockaddr& addr)
    : Context_(context), ListenSocket(std::static_pointer_cast<Socket>(socket)), ListenSocketAddr(addr)
{

}
Listener::~Listener() {}

void Listener::close()
{
    ListenSocket->close();
    IO_Context_.reset();
}

void Listener::async_accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket>&)> &&handler)
{


}
void Listener::handleaccept(epoll_event& ev)
{
    sockaddr_storage addr;
    auto conf = accept(ListenSocket->get_handle(), (sockaddr*)&addr, &addr );
    auto handle(std::move(Win_IO_Accept_Context_->completionhandler));
    Win_IO_Accept_Context_->clear();
    if (conf >0 && handle) {
        auto sock = std::make_shared<Socket>(Context_);
        sock->set_handle(conf);
        sock->setsockopt<SL::NET::SocketOptions::O_BLOCKING>(SL::NET::SockOptStatus::ENABLED);
        handle(sock);
        IO_Context_->handleaccept(conff);
    } else {
        if(conf>=0) {
            closesocket(conf);
        }
        if(handle) {
            handle(std::shared_ptr<Socket>());
        }
    }
}
} // namespace NET
} // namespace SL
