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
    std::lock_guard<spinlock> lock(Lock);
    for(auto a: OutStandingEvents) {
        Context_->PendingIO -= 1;
        delete a;
    }
    OutStandingEvents.clear();
}
void Listener::handle_accept(bool success, Win_IO_Accept_Context *context)
{
    auto &listener = *context->Listener_;
    auto &iocontext = *context->Context_;
    sockaddr_in remote = {0};
    socklen_t len = sizeof(remote);
    iocontext.PendingIO -=1;
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
        if (epoll_ctl(iocontext.IOCPHandle, EPOLL_CTL_ADD, i, &ev) == -1) {
            handler(TranslateError(), std::shared_ptr<Socket>());
        } else {
            handler(StatusCode::SC_SUCCESS, sock);
        }
    }
    {
        std::lock_guard<spinlock> lock(listener.Lock);
        listener.OutStandingEvents.erase(std::remove(std::begin(listener.OutStandingEvents), std::end(listener.OutStandingEvents), context), std::end(listener.OutStandingEvents));
    }
    delete context;
}
void Listener::start_accept(bool success, Win_IO_Accept_Context *context)
{

}
void Listener::accept(const std::function<void(StatusCode, const std::shared_ptr<ISocket> &)> &&handler)
{
    auto context = new Win_IO_Accept_Context();
    {
        std::lock_guard<spinlock> lock(Lock);
        OutStandingEvents.push_back(context);
    }
    context->completionhandler = std::move(handler);
    context->IOOperation = IO_OPERATION::IoAccept;
    context->Context_ = Context_;
    context->Listener_ = this;
    context->Family = ListenSocketAddr.get_Family();
    context->ListenSocket = ListenSocket->get_handle();
    Context_->PendingIO += 1;
    epoll_event ev = {0};
    ev.events = EPOLLIN | EPOLLONESHOT;
    ev.data.ptr = context;
    if (epoll_ctl(Context_->IOCPHandle, EPOLL_CTL_MOD, context->ListenSocket, &ev) == -1) {
        context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
        {
            std::lock_guard<spinlock> lock(Lock);
            Context_->PendingIO -= 1;
            OutStandingEvents.erase(std::remove(std::begin(OutStandingEvents), std::end(OutStandingEvents), context), std::end(OutStandingEvents));
        }
        delete context;
    }
}
}
} // namespace NET
