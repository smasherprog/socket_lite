
#include "Context.h"
#include "Socket.h"
#include <assert.h>

namespace SL
{
namespace NET
{

Socket::Socket(Context* context, AddressFamily family) : Socket(context)
{
    if (family == AddressFamily::IPV4) {
        handle = socket(AF_INET, SOCK_STREAM, 0);
    } else {
        handle = socket(AF_INET6, SOCK_STREAM, 0);
    }
}
Socket::Socket(Context* context) : Context_(context) {}
Socket::~Socket()
{
}
void Socket::connect(SL::NET::sockaddr &address, const std::function<void(StatusCode)> &&handler)
{
    ISocket::close();
    ReadContext.clear();
    if (address.get_Family() == AddressFamily::IPV4) {
        handle = socket(AF_INET, SOCK_STREAM, 0);
    } else {
        handle = socket(AF_INET6, SOCK_STREAM, 0);
    }
    if(handle==-1) {
        return handler(TranslateError());
    }
    setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING);

    ReadContext.completionhandler = [ihandle(std::move(handler))](StatusCode code, size_t) {
        ihandle(code);
    };
    ReadContext.IOOperation = IO_OPERATION::IoConnect;
    Context_->PendingIO +=1;
    auto ret = ::connect(handle, (::sockaddr*)address.get_SocketAddr(), address.get_SocketAddrLen());
    if(ret ==-1) {//will complete some time later
        auto err = errno;
        if(err != EINPROGRESS) {
            Context_->PendingIO -=1;
            auto chandle(std::move(ReadContext.completionhandler));
            ReadContext.clear();
            return chandle(TranslateError(&err), 0);
        }

        epoll_event ev = {0};
        ev.data.ptr = &ReadContext;
        ev.data.fd = handle;
        ev.events = EPOLLIN | EPOLLEXCLUSIVE | EPOLLONESHOT;

        if(epoll_ctl(Context_->iocp.handle, EPOLL_CTL_ADD, handle, &ev) == -1) {
            Context_->PendingIO -=1;
            auto chandle(std::move(ReadContext.completionhandler));
            ReadContext.clear();
            return chandle(TranslateError(), 0);
        }

    } else if(ret ==0) {//connection completed
        Context_->PendingIO -=1;
        auto chandle(std::move(ReadContext.completionhandler));
        return chandle(StatusCode::SC_SUCCESS, 0);
    }
}
void Socket::handlerecv()
{

}
void Socket::handlewrite()
{

}
void Socket::recv(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
{
    assert(ReadContext.IOOperation == IO_OPERATION::IoNone);
    epoll_event ev = {0};
    ev.data.ptr = &ReadContext;
    ev.data.fd = handle;
    ev.events = EPOLLIN | EPOLLEXCLUSIVE | EPOLLONESHOT;

    if(epoll_ctl(Context_->iocp.handle, EPOLL_CTL_MOD, handle, &ev) == -1) {
        Context_->PendingIO -=1;
        auto chandle(std::move(ReadContext.completionhandler));
        ReadContext.clear();
        return chandle(TranslateError(), 0);
    }
}
void Socket::send(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
{
    auto ret = send(handle,);

    assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
    epoll_event ev = {0};
    ev.data.ptr = &WriteContext;
    ev.data.fd = handle;
    ev.events = EPOLLOUT | EPOLLEXCLUSIVE | EPOLLONESHOT;

    if(epoll_ctl(Context_->iocp.handle, EPOLL_CTL_MOD, handle, &ev) == -1) {
        Context_->PendingIO -=1;
        auto chandle(std::move(WriteContext.completionhandler));
        WriteContext.clear();
        return chandle(TranslateError(), 0);
    }
}

} // namespace NET
} // namespace SL
