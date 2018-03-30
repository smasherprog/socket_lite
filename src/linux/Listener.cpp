#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <assert.h>
#include <sys/epoll.h>
#include <netinet/ip.h>
#include <algorithm>

namespace SL
{
namespace NET
{

Listener::Listener(Context *context, std::shared_ptr<ISocket> &&socket, const SL::NET::sockaddr &addr)
    : Context_(context), ListenSocket(std::static_pointer_cast<Socket>(socket)), ListenSocketAddr(addr)
{
    epoll_event ev = {0};
    ev.events = EPOLLONESHOT;
    if (epoll_ctl(Context_->IOCPHandle, EPOLL_CTL_ADD, ListenSocket->get_handle(), &ev) == -1) {
        abort();
    }
}
Listener::~Listener()
{
    close();
}

void Listener::close()
{
    ListenSocket->close();
    auto h = std::move(Win_IO_Accept_Context_.completionhandler);
    if(h) {
        Context_->PendingIO -= 1;
        Win_IO_Accept_Context_.reset();
        h(StatusCode::SC_CLOSED, std::shared_ptr<Socket>());
    }
}
void Listener::handle_accept(bool success, Win_IO_Accept_Context *context)
{
    sockaddr_in remote = {0};
    socklen_t len = sizeof(remote);
    context->Context_->PendingIO -=1;
    int i = ::accept(context->ListenSocket, reinterpret_cast<::sockaddr *>(&remote), &len);
    auto handler(std::move(context->completionhandler));
    if (i == -1) {
        handler(TranslateError(), std::shared_ptr<Socket>());
    } else {
        auto sock(std::make_shared<Socket>(context->Context_));
        sock->set_handle(i);
        INTERNAL::setsockopt_O_BLOCKING(i, Blocking_Options::NON_BLOCKING);
        epoll_event ev = {0};
        ev.events = EPOLLONESHOT;
        if (epoll_ctl( context->Context_->IOCPHandle, EPOLL_CTL_ADD, i, &ev) == -1) {
            handler(TranslateError(), std::shared_ptr<Socket>());
        } else {
            handler(StatusCode::SC_SUCCESS, sock);
        }
    }
}

void Listener::accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler)
{
    Win_IO_Accept_Context_.completionhandler = std::move(handler);
    Win_IO_Accept_Context_.IOOperation = IO_OPERATION::IoAccept;
    Win_IO_Accept_Context_.Context_ = Context_;
    Win_IO_Accept_Context_.Listener_ = this;
    Win_IO_Accept_Context_.Family = ListenSocketAddr.get_Family();
    Win_IO_Accept_Context_.ListenSocket = ListenSocket->get_handle();
    Context_->PendingIO += 1;
    epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.ptr = &Win_IO_Accept_Context_;
    if (epoll_ctl(Context_->IOCPHandle, EPOLL_CTL_MOD, Win_IO_Accept_Context_.ListenSocket, &ev) == -1) {
        auto h(std::move(Win_IO_Accept_Context_.completionhandler));
        if(h) {
            h(TranslateError(), std::shared_ptr<Socket>());
            Context_->PendingIO -= 1;
        }
    }
}
}
} // namespace NET
