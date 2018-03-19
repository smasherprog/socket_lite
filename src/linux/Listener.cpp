#include "Context.h"
#include "Listener.h"
#include "Socket.h"
#include <assert.h>
namespace SL {
namespace NET {

    Listener::Listener(Context *context, std::shared_ptr<ISocket> &&socket, const SL::NET::sockaddr &addr)
        : Context_(context), ListenSocket(std::static_pointer_cast<Socket>(socket)), ListenSocketAddr(addr)
    {
        epoll_event ev = {0};
        if (epoll_ctl(iocp.handle, EPOLL_CTL_ADD, listensocket->get_handle(), &ev) == -1) {
            return std::shared_ptr<Listener>();
        }
    }
    Listener::~Listener() {}

    void Listener::close()
    {
        ListenSocket->close();
        if (Win_IO_Accept_Context_.IOOperation != IO_OPERATION::IoNone) {
            Context_->PendingIO -= 1;
            auto chandle(std::move(Win_IO_Accept_Context_.completionhandler));
            Win_IO_Accept_Context_.clear();
            return chandle(StatusCode::SC_CLOSED, 0);
        }
    }
    void Listener::start_accept(bool success, Win_IO_Accept_Context *context)
    {
        sockaddr_in remote = {0};
        socklen_t len = sizeof(remote);
        int i = accept(context->ListenSocket, reinterpret_cast<::sockaddr *>(&remote), &len);
        if (i == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                handler(StatusCode::SC_CLOSED, std::shared_ptr<ISocket>());
            }
            else {
                handler(TranslateError(), std::shared_ptr<ISocket>());
            }
        }
        else {
            auto sock(std::allocate_shared<Socket>(iocontext.SocketAllocator, context->Context_));
            sock->set_handle(i);
            sock->setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING);
            epoll_event ev = {0};
            ev.events = EPOLLOUT | EPOLLIN | EPOLLEXCLUSIVE;
            ev.data.ptr = &sock->WriteContext;
            if (epoll_ctl(iocp.handle, EPOLL_CTL_ADD, i, &ev) == -1) {
                handler(TranslateError(), std::shared_ptr<ISocket>());
            }
            else {
                handler(StatusCode::SC_SUCCESS, sock);
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
        ev.events = EPOLLIN | EPOLLEXCLUSIVE;
        ev.data.ptr = context;
        if (epoll_ctl(iocp.handle, EPOLL_CTL_MOD, i, &ev) == -1) {
            Context_->PendingIO -= 1;
            context->completionhandler(TranslateError(), std::shared_ptr<Socket>());
            Context_->Win_IO_Accept_ContextAllocator.deallocate(context, 1);
        }
    }
} // namespace NET
} // namespace SL
