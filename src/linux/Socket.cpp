#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace SL
{
namespace NET
{
 
void connect(Socket &socket, SL::NET::sockaddr &address, const std::function<void(StatusCode)> &&handler)
{
    SocketGetter sg(socket);
    auto writecontext = sg.getWriteContext();
    assert(writecontext->IOOperation == INTERNAL::IO_OPERATION::IoNone);
    auto handle = writecontext->Socket_ = INTERNAL::Socket(address.get_Family());
    auto ret = ::connect(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen());
    if (ret == -1) { // will complete some time later
        auto err = errno;
        if (err != EINPROGRESS) { // error with the socket
            handler(TranslateError(&err));
        } else {
            //need to allocate for the epoll call
            sg.getPendingIO() += 1;
            writecontext->setCompletionHandler([ihandler(std::move(handler))](StatusCode s, size_t sz) {
                ihandler(s);
            });
            writecontext->IOOperation = INTERNAL::IO_OPERATION::IoConnect;

            epoll_event ev = {0};
            ev.data.ptr = writecontext;
            ev.events = EPOLLOUT | EPOLLONESHOT;
            if (epoll_ctl(sg.getIOCPHandle(), EPOLL_CTL_ADD, handle, &ev) == -1) { 
                if (auto h = writecontext->getCompletionHandler(); h) {
                    writecontext->reset();
                    sg.getPendingIO() -= 1;
                    h(TranslateError(), 0);
                }
            }
        }
    } else { // connection completed
        auto[suc, errocode] = INTERNAL::getsockopt_O_ERROR(handle);
        if (suc == StatusCode::SC_SUCCESS && errocode.has_value() && errocode.value() == 0) {
            handler(StatusCode::SC_SUCCESS);
        } else {
            handler(TranslateError());
        }
    }
}
void continue_connect(bool success, INTERNAL::Win_IO_RW_Context *context)
{
    auto h = context->getCompletionHandler();
    if (h) { 
        context->reset();
        auto[suc, errocode] = INTERNAL::getsockopt_O_ERROR(context->Socket_);
        if (suc == StatusCode::SC_SUCCESS && errocode.has_value() && errocode.value() == 0) {
            h(StatusCode::SC_SUCCESS, 0);
        } else {
            h(TranslateError(), 0);
        }
    }
}
void continue_io(bool success, INTERNAL::Win_IO_RW_Context *context, std::atomic<int> &pendingio, int iocphandle)
{
    auto bytestowrite = context->bufferlen - context->transfered_bytes;
    auto count = 0;
    if (context->IOOperation == INTERNAL::IO_OPERATION::IoRead) {
        count = ::read(context->Socket_, context->buffer + context->transfered_bytes, bytestowrite);
        if (count <= 0) { // possible error or continue
            if ((errno != EAGAIN && errno != EINTR) || count == 0) {
                if (auto h(context->getCompletionHandler()); h) { 
                    context->reset();
                    h(TranslateError(), 0);
                }
                return; // done get out
            } else {
                count =0;
            }
        }
    } else {
        count = ::write(context->Socket_, context->buffer + context->transfered_bytes, bytestowrite);
        if (count < 0) { // possible error or continue
            if (errno != EAGAIN && errno != EINTR) {
                if (auto h(context->getCompletionHandler()); h) { 
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
    ev.events = context->IOOperation == INTERNAL::IO_OPERATION::IoRead ? EPOLLIN : EPOLLOUT;
    ev.events |= EPOLLONESHOT;
    pendingio += 1;
    if (epoll_ctl(iocphandle, EPOLL_CTL_MOD, context->Socket_ , &ev) == -1) {
        auto h(context->getCompletionHandler());
        if (h) {
            context->reset(); 
            pendingio -= 1;
            h(TranslateError(), 0);
        }
    }
}
} // namespace NET
} // namespace SL
