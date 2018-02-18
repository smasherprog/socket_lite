#include "Listener.h"
#include "Socket.h"
#include <assert.h>
#include <iostream>

namespace SL
{
namespace NET
{

Listener::Listener( Context* context,std::shared_ptr<ISocket> &&socket, const SL::NET::sockaddr& addr)
    : Context_(context), ListenSocket(std::static_pointer_cast<Socket>(socket)), ListenSocketAddr(addr)
{

}
Listener::~Listener() {}

void Listener::close()
{
    ListenSocket->close();
}

void Listener::async_accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket>&)> &&handler)
{
    assert(Win_IO_Accept_Context_.IOOperation == IO_OPERATION::IoNone);
    epoll_event ev = {0};
    ev.data.ptr = this;
    ev.data.fd = ListenSocket->get_handle();
    ev.events = EPOLLIN | EPOLLEXCLUSIVE | EPOLLONESHOT;
    epoll_ctl(Context_->iocp.handle, EPOLL_CTL_ADD, ListenSocket->get_handle(), &ev) != -1;
}
}
}
