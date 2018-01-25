
#include "IO_Context.h"
#include "Listener.h"
#include "Socket.h"
#include <assert.h>
#include <iostream>

namespace SL
{
namespace NET
{
std::shared_ptr<IListener> SOCKET_LITE_EXTERN CreateListener(const std::shared_ptr<IIO_Context> &iocontext,
        std::shared_ptr<ISocket> &&listensocket)
{
    listensocker->setsockopt<Socket_Options::O_BLOCKING>(true);
    auto context = static_cast<IO_Context *>(iocontext.get());
    auto listener = std::make_shared<Listener>(context->PendingIO, std::forward<std::shared_ptr<ISocket>>(listensocket));
    context->Listener_ = listener;
    epoll_event ev;
    ev.data.fd = listener->ListenSocket->get_handle();
    ev.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE | EPOLLONESHOT;
    if(epoll_ctl(context->iocp.handle, EPOLL_CTL_ADD, listener->ListenSocket->get_handle(), &ev)==-1) {
        return std::shared_ptr<IListener>();
    }
    return listener;
}

Listener::Listener(std::atomic<size_t> &counter, std::shared_ptr<ISocket> &&socket)
    : PendingIO(counter), ListenSocket(std::static_pointer_cast<Socket>(socket))
{
}
Listener::~Listener() {}

void Listener::close()
{
    ListenSocket->close();
}

void Listener::async_accept(std::shared_ptr<ISocket> &socket, const std::function<void(bool)> &&handler)
{
    assert(Win_IO_Accept_Context_.IOOperation == IO_OPERATION::IoNone);
    Win_IO_Accept_Context_.Socket_ = std::static_pointer_cast<Socket>(socket);
    Win_IO_Accept_Context_.IOOperation = IO_OPERATION::IoAccept;
    Win_IO_Accept_Context_.ListenSocket = ListenSocket->get_handle();
    Win_IO_Accept_Context_.completionhandler = std::move(handler);

}
void Listener::handleaccept(epoll_event% ev)
{
    sockaddr_storage addr;
    auto conf = accept(ListenSocket->get_handle(), (sockaddr*)&addr, &addr );
    auto handle(std::move(Win_IO_Accept_Context_->completionhandler));
    Win_IO_Accept_Context_->clear();
    if (handle) {
        handle(success);
    }
}
} // namespace NET
} // namespace SL
