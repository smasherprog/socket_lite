#include "Socket_Lite.h"
#include <assert.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace SL
{
namespace NET
{
 

void connect(Socket &socket, SL::NET::sockaddr &address, std::function<void(StatusCode)> &&handler)
{
    SocketGetter sg(socket);
    auto handle = sg.setSocket(INTERNAL::Socket(address.get_Family()));

    auto ret = ::connect(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen());
    if (ret == -1) { // will complete some time later
        auto err = errno;
        if (err != EINPROGRESS) { // error with the socket
            handler(TranslateError(&err));
        } else {
            //need to allocate for the epoll call
            sg.getPendingIO() += 1;

            auto context = new Win_IO_Connect_Context();
            context->Context_ = sg.getContext();
            context->setCompletionHandler(std::move(handler));
            context->IOOperation = INTERNAL::IO_OPERATION::IoConnect;
            context->Socket_ = handle; 

            epoll_event ev = {0};
            ev.data.ptr = &WriteContext;
            ev.events = EPOLLOUT | EPOLLONESHOT;
            if (epoll_ctl(sg.getIOCPHandle(), EPOLL_CTL_ADD, handle, &ev) == -1) {
                if (auto h(context->getCompletionHandler()); h) {
                    sg.getPendingIO() -= 1;
                    delete context;
                    h(TranslateError());
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
void continue_connect(bool success, Win_IO_Connect_Context *context)
{
    auto h = context->getCompletionHandler();
    auto handle = context->Socket_;
    delete context;
    if (h) {  
        auto[suc, errocode] = INTERNAL::setsockopt_factory_impl<SL::NET::SocketOptions::O_ERROR>::setsockopt_(handle);
        if (suc == StatusCode::SC_SUCCESS && errocode.has_value() && errocode.value() == 0) {
            h(StatusCode::SC_SUCCESS);
        } else {
            h(TranslateError());
        }
    }
}

void continue_io(bool success, Win_IO_RW_Context *context)
{
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
