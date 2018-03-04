
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
template <typename T> void IOComplete(Socket* s, StatusCode code, size_t bytes, T* context)
{
    if(code != StatusCode::SC_SUCCESS) {
        s->close();
        bytes=0;
    }
    auto handler(std::move(context->completionhandler));
    context->clear();
    handler(code, bytes);
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
            return IOComplete(this, TranslateError(&err), 0, &ReadContext);
        }

        epoll_event ev = {0};
        ev.data.ptr = &ReadContext;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLONESHOT;
        if(epoll_ctl(Context_->iocp.handle, EPOLL_CTL_ADD, handle, &ev) == -1) {
            Context_->PendingIO -=1;
            return IOComplete(this, TranslateError(), 0, &ReadContext);
        }

    } else {//connection completed
        Context_->PendingIO -=1;
        return IOComplete(this, StatusCode::SC_SUCCESS, 0, &ReadContext);
    }
}
void Socket::handleconnect()
{
    assert(ReadContext.completionhandler);
    auto handle(std::move(ReadContext.completionhandler));
    auto [success, errocode] = getsockopt<SocketOptions::O_ERROR>();
    if(errocode.has_value()) {
        handle(StatusCode::SC_SUCCESS, 0);
    } else {
        auto erval = errocode.value();
        handle(TranslateError(&erval), 0);
    }
}
void Socket::onRecvReady()
{
    auto bytestowrite = ReadContext.bufferlen - ReadContext.transfered_bytes;
    auto count = read (handle, ReadContext.buffer + ReadContext.transfered_bytes, bytestowrite);
    if (count < 0) {//possible error or continue
        if(errno == EAGAIN || errno == EINTR) {
            continue_recv();
        } else {
            return IOComplete(this, StatusCode::SC_CLOSED, 0, &ReadContext);
        }
    } else {
        ReadContext.transfered_bytes += count;
        continue_recv();
    }

}
void Socket::continue_recv()
{
    if(ReadContext.transfered_bytes == ReadContext.bufferlen) {
        //done writing
        return IOComplete(this, StatusCode::SC_SUCCESS, ReadContext.transfered_bytes, &ReadContext);
    }
    epoll_event ev = {0};
    ev.data.ptr = &ReadContext;
    ev.events = EPOLLIN | EPOLLEXCLUSIVE | EPOLLONESHOT;
    Context_->PendingIO +=1;
    if(epoll_ctl(Context_->iocp.handle, EPOLL_CTL_ADD, handle, &ev) == -1) {
        Context_->PendingIO -=1;
        IOComplete(this, StatusCode::SC_CLOSED, 0, &ReadContext);
    }
}
void Socket::recv(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
{
    assert(ReadContext.IOOperation == IO_OPERATION::IoNone);
    ReadContext.buffer = buffer;
    ReadContext.bufferlen = buffer_size;
    ReadContext.IOOperation = IO_OPERATION::IoRead;
    ReadContext.Socket_ =  this;
    ReadContext.completionhandler = std::move(handler);
    onRecvReady();
}


void Socket::onSendReady()
{
    auto bytestowrite = WriteContext.bufferlen - WriteContext.transfered_bytes;
    auto count = ::write (handle, WriteContext.buffer + WriteContext.transfered_bytes, bytestowrite);
    if ( count <0) {//possible error or continue
        if(errno == EAGAIN || errno == EINTR) {
            continue_send();
        } else {
            return IOComplete(this, StatusCode::SC_CLOSED, 0, &WriteContext);
        }
    } else {
        WriteContext.transfered_bytes += count;
        continue_send();
    }
}
void Socket::continue_send()
{
    if(WriteContext.transfered_bytes == WriteContext.bufferlen) {
        //done writing
        return IOComplete(this, StatusCode::SC_SUCCESS, WriteContext.transfered_bytes, &WriteContext);
    }
    epoll_event ev = {0};
    ev.data.ptr = &WriteContext;
    ev.events = EPOLLOUT | EPOLLEXCLUSIVE | EPOLLONESHOT;
    Context_->PendingIO +=1;
    if(epoll_ctl(Context_->iocp.handle, EPOLL_CTL_ADD, handle, &ev) == -1) {
        Context_->PendingIO -=1;
        IOComplete(this, StatusCode::SC_CLOSED, 0, &WriteContext);
    }
}
void Socket::send(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
{
    assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
    WriteContext.buffer = buffer;
    WriteContext.bufferlen = buffer_size;
    WriteContext.IOOperation = IO_OPERATION::IoWrite;
    WriteContext.Socket_ =  this;
    WriteContext.completionhandler = std::move(handler);
    onSendReady();

}
} // namespace NET
} // namespace SL
