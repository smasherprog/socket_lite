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
}

void Listener::async_accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket>&)> &&handler)
{


}
void Listener::handleaccept(epoll_event& ev)
{
}
}
}
