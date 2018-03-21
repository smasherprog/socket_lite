
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
Socket::~Socket() {}

void Socket::connect(SL::NET::sockaddr &address, const std::function<void(StatusCode)> &&handler)
{
    auto context = Context_->Win_IO_Connect_ContextAllocator.allocate(1);
    context->completionhandler = std::move(handler);
    context->IOOperation = IO_OPERATION::IoInitConnect;
    context->Socket_ = this;
    context->address = address;
    context->Context_ = Context_;
    ISocket::close();
    handle = INTERNAL::Socket(address.get_Family());
    setsockopt<SocketOptions::O_BLOCKING>(Blocking_Options::NON_BLOCKING);
    Context_->PendingIO += 1;
    auto ret = ::connect(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen());
    if (ret == -1) { // will complete some time later
        auto err = errno;
        if (err != EINPROGRESS) {
            Context_->PendingIO -= 1;
            context->completionhandler(TranslateError(&err));
            return Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
        epoll_event ev = {0};
        ev.data.ptr = context;
        ev.events = EPOLLOUT | EPOLLONESHOT;
        if (epoll_ctl(Context_->IOCPHandle, EPOLL_CTL_ADD, handle, &ev) == -1) {
            Context_->PendingIO -= 1;
            context->completionhandler(TranslateError());
            return Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
        }
    } else { // connection completed
        Context_->PendingIO -= 1;
        context->completionhandler(StatusCode::SC_SUCCESS);
        return Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
    }
}
void Socket::continue_connect(bool success, Win_IO_Connect_Context *context)
{
    auto[suc, errocode] = context->Socket_->getsockopt<SocketOptions::O_ERROR>();
    if (suc == StatusCode::SC_SUCCESS && errocode.has_value() && errocode.value() == 0) {
        context->completionhandler(StatusCode::SC_SUCCESS);
        return context->Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
    }
    auto erval = errocode.value();
    context->completionhandler(TranslateError(&erval));
    context->Context_->Win_IO_Connect_ContextAllocator.deallocate(context, 1);
}
// void Socket::handlerecv()
//{
//    if(!ReadContext.completionhandler) return;
//    auto bytestowrite = ReadContext.bufferlen - ReadContext.transfered_bytes;
//    auto count = ::read (handle, ReadContext.buffer + ReadContext.transfered_bytes, bytestowrite);
//    if (count <= 0) {//possible error or continue
//        if(count == -1 && (errno == EAGAIN || errno == EINTR)) {
//            continue_recv();
//        } else {
//            return IOComplete(this, StatusCode::SC_CLOSED, 0, &ReadContext);
//        }
//    } else {
//        ReadContext.transfered_bytes += count;
//        continue_recv();
//    }
//
//}
// void Socket::continue_recv()
//{
//    if(ReadContext.transfered_bytes == ReadContext.bufferlen) {
//        //done writing
//        return IOComplete(this, StatusCode::SC_SUCCESS, ReadContext.transfered_bytes, &ReadContext);
//    }
//    Context_->PendingIO +=1;
//
//}
void Socket::recv(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
{

    // handlerecv();
}

//
// void Socket::handlewrite()
//{
//    if(!WriteContext.completionhandler) return;
//    auto bytestowrite = WriteContext.bufferlen - WriteContext.transfered_bytes;
//    auto count = ::write (handle, WriteContext.buffer + WriteContext.transfered_bytes, bytestowrite);
//    if ( count <0) {//possible error or continue
//        if(errno == EAGAIN || errno == EINTR) {
//            continue_send();
//        } else {
//            return IOComplete(this, StatusCode::SC_CLOSED, 0, &WriteContext);
//        }
//    } else {
//        WriteContext.transfered_bytes += count;
//        continue_send();
//    }
//}
void Socket::init_connect(bool success, Win_IO_Connect_Context *context)
{

}
void Socket::continue_io(bool success, Win_IO_RW_Context *context)
{
    // if(WriteContext.transfered_bytes == WriteContext.bufferlen) {
    //    //done writing
    //    return IOComplete(this, StatusCode::SC_SUCCESS, WriteContext.transfered_bytes, &WriteContext);
    //}
    // Context_->PendingIO +=1;
}
void Socket::send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
{

    // handlewrite();
}
} // namespace NET
} // namespace SL
