
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
    auto context = std::static_pointer_cast<IO_Context *>(iocontext);
    auto addr = listensocket->getpeername();
    if(!addr.has_value()) {
        return std::shared_ptr<IListener>();
    }
    auto listener = std::make_shared<Listener>(context, std::forward<std::shared_ptr<ISocket>>(listensocket), addr.value());

    epoll_event ev;
    ev.data.fd = listener->ListenSocket->get_handle();
    ev.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE | EPOLLONESHOT;
    if(epoll_ctl(listener->iocp.handle, EPOLL_CTL_ADD, listener->ListenSocket->get_handle(), &ev)==-1) {
        return std::shared_ptr<IListener>();
    }
    return listener;
}

Listener::Listener(const std::shared_ptr<IO_Context>& context, std::shared_ptr<ISocket> &&socket, const sockaddr& addr)
    : PendingIO(context->PendingIO), ListenSocket(std::static_pointer_cast<Socket>(socket)), ListenSocketAddr(addr)
{
    AcceptThread = std::thread([&] {
        std::vecdtor<epoll_event> epollevents;
        epollevents.resize(MAXEVENTS);
        while (true) {
            int efd =-1;
            auto count = epoll_wait(iocp.handle, epollevents.data(),MAXEVENTS, -1 );
            for(auto i=0; i< count ; i++) {
                if(epolllevents[i].data.fd == ListenSocket->get_handle()) {
                    handleaccept(epolllevents[i]);
                }
            }
        }
    });
}
Listener::~Listener() {}

void Listener::close()
{
    ListenSocket->close();
}

void Listener::async_accept(const std::function<void(bool)> &&handler)
{
    assert(Win_IO_Accept_Context_.IOOperation == IO_OPERATION::IoNone);
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
    if (conf >0 && handle) {
        auto sock = std::make_shared<Socket>(PendingIO);
        sock->set_handle(conf);
        sock->setsockopt<SL::NET::O_BLOCKING>(true);
        handle(sock);
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
