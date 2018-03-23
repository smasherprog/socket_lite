
#include "Context.h"
#include "Socket.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

namespace SL
{
namespace NET
{

Socket::Socket(Context *context, AddressFamily family) : Socket(context)
{
    handle = INTERNAL::Socket(family);
}
Socket::Socket(Context *context) : Context_(context) {}
Socket::~Socket() { }
void Socket::close()
{
    ISocket::close();
    auto whandler(std::move(WriteContext.completionhandler));
    if(whandler) {
        Context_->PendingIO -= 1;
        WriteContext.reset();
        whandler->handle(StatusCode::SC_CLOSED, 0, true);
    }
    auto whandler1(std::move(ReadContext.completionhandler));
    if(whandler1) {
        Context_->PendingIO -= 1;
        WriteContext.reset();
        whandler1->handle(StatusCode::SC_CLOSED, 0, true);
    }
}
void Socket::connect(SL::NET::sockaddr &address, const std::function<void(StatusCode)> &&handler)
{
    WriteContext.completionhandler = std::make_shared<RW_CompletionHandler>();
    WriteContext.completionhandler->completionhandler = [ihandler(std::move(handler))](StatusCode s, size_t sz) {
        ihandler(s);
    };
    WriteContext.IOOperation = IO_OPERATION::IoConnect;
    WriteContext.Socket_ = this;
    WriteContext.Context_ = Context_;
    ISocket::close();
    handle = INTERNAL::Socket(address.get_Family());
    setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING);
    Context_->PendingIO += 1;
    auto ret = ::connect(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen());
    if (ret == -1) { // will complete some time later
        auto err = errno;
        if (err != EINPROGRESS) {
            Context_->PendingIO -= 1;
            auto handler(std::move(WriteContext.completionhandler->completionhandler));
            WriteContext.reset();
            return handler(TranslateError(&err),0);
        }
        epoll_event ev = {0};
        ev.data.ptr = &WriteContext;
        ev.events = EPOLLOUT | EPOLLONESHOT;
        if (epoll_ctl(Context_->IOCPHandle, EPOLL_CTL_ADD, handle, &ev) == -1) {
            Context_->PendingIO -= 1;
            auto handler(std::move(WriteContext.completionhandler->completionhandler));
            WriteContext.reset();
            return handler(TranslateError(), 0);
        }
    } else { // connection completed
        Context_->PendingIO -= 1;
        Socket::continue_connect(true, &WriteContext);
    }
}
void Socket::continue_connect(bool success, Win_IO_RW_Context *context)
{
    auto handler(std::move(context->completionhandler->completionhandler));
    auto[suc, errocode] = context->Socket_->getsockopt<SocketOptions::O_ERROR>();
    if (suc == StatusCode::SC_SUCCESS && errocode.has_value() && errocode.value() == 0) {
        context->reset();
        return handler(StatusCode::SC_SUCCESS, 0);
    }
    auto erval = errocode.value();
    context->reset();
    handler(TranslateError(&erval), 0);
}

void Socket::send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
{
    assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
    WriteContext.buffer = buffer;
    WriteContext.bufferlen =  buffer_size;
    WriteContext.completionhandler = std::move(handler);
    WriteContext.Context_ = Context_;
    WriteContext.IOOperation = IO_OPERATION::IoWrite;
    WriteContext.Socket_ = this;
    WriteContext.transfered_bytes =0;
    continue_io(true, &WriteContext);
}
void Socket::recv(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
{
    assert(ReadContext.IOOperation == IO_OPERATION::IoNone);
    ReadContext.buffer = buffer;
    ReadContext.bufferlen =  buffer_size;
    ReadContext.completionhandler = std::move(handler);
    ReadContext.Context_ = Context_;
    ReadContext.IOOperation = IO_OPERATION::IoRead;
    ReadContext.Socket_ = this;
    ReadContext.transfered_bytes =0;
    continue_io(true, &ReadContext);
}

void Socket::continue_io(bool success, Win_IO_RW_Context *context)
{
    auto bytestowrite = context->bufferlen - context->transfered_bytes;
    if(context->IOOperation == IO_OPERATION::IoRead) {
        auto count = ::read (context->Socket_->get_handle(), context->buffer + context->transfered_bytes, bytestowrite);
        if (count <= 0) {//possible error or continue
            if(count == -1 && (errno == EAGAIN || errno == EINTR)) {

            } else {
                return IOComplete(this, StatusCode::SC_CLOSED, 0, &ReadContext);
            }
        } else {
            ReadContext.transfered_bytes += count;
        }


    } else {
        auto count = ::write (context->Socket_->get_handle(), context->buffer + context->transfered_bytes, bytestowrite);
        if ( count <0) {//possible error or continue
            if(errno == EAGAIN || errno == EINTR) {
                continue_send();
            } else {
                return IOComplete(this, StatusCode::SC_CLOSED, 0, &WriteContext);
            }
        } else {
            context->transfered_bytes += count;
            continue_send();
        }
    }
}
} // namespace NET
} // namespace SL
