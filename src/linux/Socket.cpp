
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

    if(!UpdateEpoll(handle, Context_->iocp.handle, &ReadContext)) {
        return handler(TranslateError());
    }
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
            return chandle(TranslateError(&err), 0);
        }
    } else if(ret ==0) {//connection completed
        Context_->PendingIO -=1;
        auto chandle(std::move(ReadContext.completionhandler));
        return chandle(StatusCode::SC_SUCCESS, 0);
    }
}
void Socket::handlerecv(bool success, Win_IO_RW_Context* context)
{

}
void Socket::handlewrite(bool success, Win_IO_RW_Context* context)
{

}
void Socket::recv(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
{

}
void Socket::send(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
{

}

} // namespace NET
} // namespace SL
