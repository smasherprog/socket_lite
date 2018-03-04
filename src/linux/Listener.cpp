#include "Listener.h"
#include "Socket.h"
#include <assert.h>
#include <iostream>
#include "Context.h"
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
    if(Win_IO_Accept_Context_.IOOperation != IO_OPERATION::IoNone) {
        Context_->PendingIO -=1;
        auto chandle(std::move(Win_IO_Accept_Context_.completionhandler));
        Win_IO_Accept_Context_.clear();
        return chandle(StatusCode::SC_CLOSED, 0);
    }
}

void Listener::async_accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket>&)> &&handler)
{
    assert(Win_IO_Accept_Context_.IOOperation == IO_OPERATION::IoNone);
    Win_IO_Accept_Context_.IOOperation = IO_OPERATION::IoAccept;
    Win_IO_Accept_Context_.ListenSocket =ListenSocket->get_handle();
    Win_IO_Accept_Context_.completionhandler= std::move(handler);

    epoll_event ev = {0};
    ev.data.ptr = &Win_IO_Accept_Context_;
    ev.events = EPOLLIN | EPOLLONESHOT;
    Context_->PendingIO +=1;
    if(epoll_ctl(Context_->iocp.handle, EPOLL_CTL_MOD, ListenSocket->get_handle(), &ev)  == -1) {
        Context_->PendingIO -=1;
        auto chandle(std::move(Win_IO_Accept_Context_.completionhandler));
        Win_IO_Accept_Context_.clear();
        return chandle(TranslateError(), 0);
    }
}
}
}
