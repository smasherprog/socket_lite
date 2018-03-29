
#include "Context.h"
#include "Socket.h"
#include <assert.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace SL
{
namespace NET
{

Socket::Socket(Context *context, AddressFamily family) : Socket(context)
{
    handle = INTERNAL::Socket(family);
}
Socket::Socket(Context *context) : Context_(context)
{
    WriteContext.Context_ = ReadContext.Context_ = Context_;
    WriteContext.Socket_ = ReadContext.Socket_ = this;
}
Socket::~Socket()
{
    Socket::close();
}
void Socket::close()
{
    ISocket::close();
    auto whandler(WriteContext.getCompletionHandler());
    if (whandler) {
        WriteContext.reset();
        Context_->PendingIO -= 1;
        whandler(StatusCode::SC_CLOSED, 0);
    }
    auto whandler1(ReadContext.getCompletionHandler());
    if (whandler1) {
        ReadContext.reset();
        Context_->PendingIO -= 1;
        whandler1(StatusCode::SC_CLOSED, 0);
    }
}
void Socket::connect(SL::NET::sockaddr &address, const std::function<void(StatusCode)> &&handler)
{
    ISocket::close();
    handle = INTERNAL::Socket(address.get_Family());
    auto ret = ::connect(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen());
    if (ret == -1) { // will complete some time later
        auto err = errno;
        if (err != EINPROGRESS) { // error with the socket
            handler(TranslateError(&err));
        } else {
            //need to allocate for the epoll call
            Context_->PendingIO += 1;
            WriteContext.setCompletionHandler([ihandler(std::move(handler))](StatusCode s, size_t sz) {
                ihandler(s);
            });
            WriteContext.IOOperation = IO_OPERATION::IoConnect;

            epoll_event ev = {0};
            ev.data.ptr = &WriteContext;
            ev.events = EPOLLOUT | EPOLLONESHOT;
            if (epoll_ctl(Context_->IOCPHandle, EPOLL_CTL_ADD, handle, &ev) == -1) {
                auto h = WriteContext.getCompletionHandler();
                if (h) {
                    WriteContext.reset();
                    Context_->PendingIO -= 1;
                    h(TranslateError(), 0);
                }
            }
        }
    } else { // connection completed
        auto[suc, errocode] = getsockopt<SocketOptions::O_ERROR>();
        if (suc == StatusCode::SC_SUCCESS && errocode.has_value() && errocode.value() == 0) {
            handler(StatusCode::SC_SUCCESS);
        } else {
            handler(TranslateError());
        }
    }
}
void Socket::continue_connect(bool success, Win_IO_RW_Context *context)
{
    auto h = context->getCompletionHandler();
    if (h) {
        context->Context_->PendingIO -= 1;
        context->reset();
        auto[suc, errocode] = context->Socket_->getsockopt<SocketOptions::O_ERROR>();
        if (suc == StatusCode::SC_SUCCESS && errocode.has_value() && errocode.value() == 0) {
            h(StatusCode::SC_SUCCESS, 0);
        } else {
            h(TranslateError(), 0);
        }
    }
}

void Socket::send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
{
    assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
    WriteContext.transfered_bytes = 0;
    auto count = ::write(handle, buffer, buffer_size);
    if (count < 0) { // possible error or continue
        if (errno != EAGAIN && errno != EINTR) {
            close();
            return handler(TranslateError(), 0);
        }
    } else {
        WriteContext.transfered_bytes = static_cast<size_t>(count);
    }
    if (count > 0 && static_cast<size_t>(count) == buffer_size) {
        return handler(StatusCode::SC_SUCCESS, buffer_size);
    }
    WriteContext.buffer = buffer;
    WriteContext.bufferlen = buffer_size;
    WriteContext.setCompletionHandler(std::move(handler));
    WriteContext.IOOperation = IO_OPERATION::IoWrite;
    Context_->PendingIO += 1;
    continue_io(true, &WriteContext);
}
void Socket::recv(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
{
    assert(ReadContext.IOOperation == IO_OPERATION::IoNone);
    ReadContext.transfered_bytes = 0;
    auto count = ::read(handle, buffer, buffer_size);
    if (count <= 0) { // possible error or continue
        if ((errno != EAGAIN && errno != EINTR) || count == 0) {
            close();
            return handler(TranslateError(), 0);
        }
    } else {
        ReadContext.transfered_bytes = static_cast<size_t>(count);
    }
    if (count>0 && static_cast<size_t>(count) == buffer_size) {
        return handler(StatusCode::SC_SUCCESS, buffer_size);
    }
    ReadContext.buffer = buffer;
    ReadContext.bufferlen = buffer_size;
    ReadContext.setCompletionHandler(std::move(handler));
    ReadContext.IOOperation = IO_OPERATION::IoRead;

    Context_->PendingIO += 1;
    continue_io(true, &ReadContext);
}

void Socket::continue_io(bool success, Win_IO_RW_Context *context)
{
    context->Context_->PendingIO -= 1;
    auto bytestowrite = context->bufferlen - context->transfered_bytes;
    auto count = 0;
    if (context->IOOperation == IO_OPERATION::IoRead) {
        count = ::read(context->Socket_->get_handle(), context->buffer + context->transfered_bytes, bytestowrite);
        if (count <= 0) { // possible error or continue
            if ((errno != EAGAIN && errno != EINTR) || count == 0) {
                auto h(context->getCompletionHandler());
                if (h) {
                    context->Socket_->close();
                    context->reset();
                    h(TranslateError(), 0);
                }
                return; // done get out
            } else {
                count =0;
            }
        }
    } else {
        count = ::write(context->Socket_->get_handle(), context->buffer + context->transfered_bytes, bytestowrite);
        if (count < 0) { // possible error or continue
            if (errno != EAGAIN && errno != EINTR) {
                auto h(context->getCompletionHandler());
                if (h) {
                    context->Socket_->close();
                    context->reset();
                    h(TranslateError(), 0);
                }
                return; // done get out
            } else {
                count =0;
            }
        }
    }
    context->transfered_bytes += count;
    if (context->transfered_bytes == context->bufferlen) {
        auto h(context->getCompletionHandler());
        if (h) {
            context->reset();
            h(StatusCode::SC_SUCCESS, context->bufferlen);
        }
        return; // done with this get out!!
    }

    epoll_event ev = {0};
    ev.data.ptr = context;
    ev.events = context->IOOperation == IO_OPERATION::IoRead ? EPOLLIN : EPOLLOUT;
    ev.events |= EPOLLONESHOT;
    context->Context_->PendingIO += 1;
    if (epoll_ctl(context->Context_->IOCPHandle, EPOLL_CTL_MOD, context->Socket_->get_handle(), &ev) == -1) {
        auto h(context->getCompletionHandler());
        if (h) {
            context->reset();
            context->Socket_->close();
            context->Context_->PendingIO -= 1;
            h(TranslateError(), 0);
        }
    }
}
} // namespace NET
} // namespace SL
