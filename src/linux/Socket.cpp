
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
    int done = 0;
    while (true) {
        auto count = read (handle, ReadContext.buffer + ReadContext.transfered_bytes, ReadContext.bufferlen - ReadContext.transfered_bytes);
        if (count == -1) {
            /* If errno == EAGAIN, that means we have read all
               data. So go back to the main loop. */
            if (errno == EAGAIN) {
                epoll_event ev = {0};
                ev.data.ptr = &ReadContext;
                ev.data.fd = handle;
                ev.events = EPOLLIN | EPOLLEXCLUSIVE | EPOLLONESHOT;
                ReadContext.IOOperation = IO_OPERATION::IoRead;
                Context_->PendingIO +=1;
                if(epoll_ctl(Context_->iocp.handle, EPOLL_CTL_MOD, handle, &ev) == -1) {
                    Context_->PendingIO -=1;
                    auto chandle(std::move(ReadContext.completionhandler));
                    ReadContext.clear();
                    return chandle(TranslateError(), 0);
                }
                break;
            }
            break;
        } else if (count == 0) {
            /* End of file. The remote has closed the
               connection. */
            ISocket::close();
            auto chandle(std::move(ReadContext.completionhandler));
            return chandle(StatusCode::SC_CLOSED, 0);
        }
    }
    if(ReadContext.transfered_bytes == ReadContext.bufferlen) {
        auto chandle(std::move(ReadContext.completionhandler));
        return chandle(StatusCode::SC_SUCCESS, ReadContext.bufferlen);
    }
}

void Socket::handleconnect()
{
    auto handle(std::move(ReadContext.completionhandler));
    auto [success, errocode] = getsockopt<SocketOptions::O_ERROR>();
    if(errocode.has_value()) {
        handle(StatusCode::SC_SUCCESS, 0);
    } else {
        auto erval = errocode.value();
        handle(TranslateError(&erval), 0);
    }
}
void Socket::recv(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
{
    assert(ReadContext.IOOperation == IO_OPERATION::IoNone);
    epoll_event ev = {0};
    ev.data.ptr = &ReadContext;
    ev.data.fd = handle;
    ev.events = EPOLLIN | EPOLLEXCLUSIVE | EPOLLONESHOT;
    ReadContext.buffer = buffer;
    ReadContext.bufferlen = buffer_size;
    ReadContext.IOOperation = IO_OPERATION::IoRead;
    ReadContext.Socket_ =  this;
    Context_->PendingIO +=1;
    if(epoll_ctl(Context_->iocp.handle, EPOLL_CTL_MOD, handle, &ev) == -1) {
        Context_->PendingIO -=1;
        ISocket::close();
        auto chandle(std::move(ReadContext.completionhandler));
        ReadContext.clear();
        return chandle(TranslateError(), 0);
    }
}


void Socket::onSendReady()
{
    auto bytestowrite = WriteContext.bufferlen - WriteContext.transfered_bytes;
    auto count = ::write (handle, WriteContext.buffer + WriteContext.transfered_bytes, bytestowrite);
    if ( count == <0) {//possible error or continue
        if(errno == EAGAIN || errno == EINTR) {
            continue_write();
        } else {
            return IOComplete(this, StatusCode::SC_CLOSED, 0, WriteContext);
        }
    } else {
        WriteContext.transfered_bytes += count;
        continue_write();
    }
}
void Socket::continue_send()
{
    if(WriteContext.transfered_bytes == WriteContext.bufferlen) {
        //done writing
        return IOComplete(this, StatusCode::SC_SUCCESS, WriteContext.transfered_bytes, WriteContext);
    }
    epoll_event ev = {0};
    ev.data.ptr = &WriteContext;
    ev.data.fd = handle;
    ev.events = EPOLLOUT | EPOLLEXCLUSIVE | EPOLLONESHOT;
    Context_->PendingIO +=1;
    if(epoll_ctl(Context_->iocp.handle, EPOLL_CTL_ADD, handle, &ev) == -1) {
        Context_->PendingIO -=1;
        IOComplete(this, StatusCode::SC_CLOSED, 0, WriteContext);
    }
}
void Socket::send(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
{
    assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
    WriteContext.buffer = buffer;
    WriteContext.bufferlen = buffer_size;
    WriteContext.IOOperation = IO_OPERATION::IoWrite;
    WriteContext.Socket_ =  this;
    onSendReady();

}
} // namespace NET
} // namespace SL
