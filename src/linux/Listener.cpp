#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <assert.h>
#include <sys/epoll.h>
#include <netinet/ip.h>

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
Listener::~Listener() {}

void Listener::close()
{
    ListenSocket->close();
}
void Listener::start_accept(bool success, Win_IO_Accept_Context *context)
{
    sockaddr_in remote = {0};
    socklen_t len = sizeof(remote);
    int i = ::accept(context->ListenSocket, reinterpret_cast<::sockaddr *>(&remote), &len);
    if (i == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            context->completionhandler(StatusCode::SC_CLOSED, std::shared_ptr<Socket>());
            context->Context_->Win_IO_Accept_ContextAllocator.deallocate(context, 1);
        } else {
            context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
            context->Context_->Win_IO_Accept_ContextAllocator.deallocate(context, 1);
        }
    } else {
        auto sock(std::allocate_shared<Socket>(context->Context_->SocketAllocator, context->Context_));
        sock->set_handle(i);
        sock->setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING);
        epoll_event ev = {0};
        ev.events = EPOLLONESHOT;
        if (epoll_ctl(context->Context_->IOCPHandle, EPOLL_CTL_ADD, i, &ev) == -1)

        {
            context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
            context->Context_->Win_IO_Accept_ContextAllocator.deallocate(context, 1);
        } else {
            context->completionhandler(StatusCode::SC_SUCCESS, sock);
            context->Context_->Win_IO_Accept_ContextAllocator.deallocate(context, 1);
        }
    }
}
void Listener::handle_accept(bool success, Win_IO_Accept_Context *context) {}
void Listener::accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler)
{

    auto context = Context_->Win_IO_Accept_ContextAllocator.allocate(1);
    context->completionhandler = std::move(handler);
    context->IOOperation = IO_OPERATION::IoStartAccept;
    context->Context_ = Context_;
    context->Family = ListenSocketAddr.get_Family();
    context->ListenSocket = ListenSocket->get_handle();
    Context_->PendingIO += 1;
    epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.ptr = context;
    if (epoll_ctl(Context_->IOCPHandle, EPOLL_CTL_MOD, context->ListenSocket, &ev) == -1) {
        Context_->PendingIO -= 1;
        context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
        Context_->Win_IO_Accept_ContextAllocator.deallocate(context, 1);
    }
}
} // namespace NET
} // namespace SL
