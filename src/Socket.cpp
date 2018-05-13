
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>
#include <type_traits>
#include <variant>

#if _WIN32
#include <Ws2ipdef.h>
#else
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace SL {
namespace NET {
    //    void processItem(std::variant<Win_IO_Context, Win_IO_Connect_Context1> &item)
    //    {
    //        /*    std::visit(
    //                [](auto &&arg) {
    //                    using T = std::decay_t<decltype(arg)>;
    //                    if constexpr (std::is_same_v<T, Win_IO_Context>) {
    //                        continue_io(true, arg);
    //                    }
    //                    else if constexpr (std::is_same_v<T, Win_IO_Connect_Context>) {
    //                        continue_connect(true, arg);
    //                    }
    //                    else {
    //                        static_assert(always_false<T>::value, "non-exhaustive visitor!");
    //                    }
    //                },
    //                item);*/
    //    }

    Socket::Socket(ContextImpl &c) : Context_(c), ReadContext_(new Win_IO_Context()), WriteContext(new Win_IO_Context()) {}
    Socket::Socket(ContextImpl &c, PlatformSocket &&p) : Socket(c){ PlatformSocket_ = std::move(p); }
    Socket::Socket(Context &c, PlatformSocket &&p) : Socket(*c.ContextImpl_, std::move(p)) {}
    Socket::Socket(Context &c) : Socket(*c.ContextImpl_) {}
    Socket::Socket(Socket &&sock) : Socket(sock.Context_, std::move(sock.PlatformSocket_)) {}
    Socket::~Socket()
    {
        close();
        if (ReadContext_) {
            delete ReadContext_;
        }
        if (WriteContext) {
            delete WriteContext;
        }
    }
    void Socket::close()
    {
        PlatformSocket_.close();
        if (ReadContext_) {
            if (auto handler(ReadContext_->getCompletionHandler()); handler) {
                 Context_.DecrementPendingIO();
                handler(StatusCode::SC_CLOSED, 0);
            }
        }
        if (WriteContext) {
            if (auto handler(WriteContext->getCompletionHandler()); handler) {
                 Context_.DecrementPendingIO();
                handler(StatusCode::SC_CLOSED, 0);
            }
        }
    }
    void Socket::recv_async(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        ReadContext_->buffer = buffer;
        ReadContext_->transfered_bytes = 0;
        ReadContext_->bufferlen = buffer_size;
        ReadContext_->Context_ = &Context_;
        ReadContext_->Socket_ = &PlatformSocket_;
        ReadContext_->setCompletionHandler(std::move(handler));
        ReadContext_->IOOperation = IO_OPERATION::IoRead;
      
#ifndef _WIN32
        epoll_event ev = {0};
        ev.data.ptr = ReadContext_;
        ev.events =  EPOLLIN | EPOLLONESHOT;
        Context_.IncrementPendingIO();
        if (epoll_ctl(Context_.IOCPHandle, EPOLL_CTL_MOD, PlatformSocket_.Handle().value, &ev) == -1) {
            if (auto h(ReadContext_->getCompletionHandler()); h) {
                Context_.DecrementPendingIO();
                h(TranslateError(), 0);
            }
        }
#else
        ReadContext_->Overlapped = {0};
        continue_io(true, ReadContext_);
#endif
    }
    void Socket::send_async(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        WriteContext->buffer = buffer;
        WriteContext->transfered_bytes = 0;
        WriteContext->bufferlen = buffer_size;
        WriteContext->Context_ = &Context_;
        WriteContext->Socket_ = &PlatformSocket_;
        WriteContext->setCompletionHandler(std::move(handler));
        WriteContext->IOOperation = IO_OPERATION::IoWrite;
        
#ifndef _WIN32
        epoll_event ev = {0};
        ev.data.ptr = WriteContext;
        ev.events =  EPOLLOUT | EPOLLONESHOT;
        Context_.IncrementPendingIO();
        if (epoll_ctl(Context_.IOCPHandle, EPOLL_CTL_MOD, PlatformSocket_.Handle().value, &ev) == -1) {
            if (auto h(WriteContext->getCompletionHandler()); h) {
              Context_.DecrementPendingIO();
                h(TranslateError(), 0);
            }
        }
#else
        WriteContext->Overlapped = {0};
        continue_io(true, WriteContext);
#endif
    }
#if _WIN32
    void continue_io(bool success, Win_IO_Context *context)
    {
        if (!success) {
            if (auto handler(context->getCompletionHandler()); handler) {
                handler(TranslateError(), 0);
            }
        }
        else if (context->bufferlen == context->transfered_bytes) {
            if (auto handler(context->getCompletionHandler()); handler) {
                handler(StatusCode::SC_SUCCESS, context->transfered_bytes);
            }
        }
        else {
            WSABUF wsabuf;
            auto bytesleft = static_cast<decltype(wsabuf.len)>(context->bufferlen - context->transfered_bytes);
            wsabuf.buf = (char *)context->buffer + context->transfered_bytes;
            wsabuf.len = bytesleft;
            DWORD dwSendNumBytes(0), dwFlags(0);
            context->Context_->IncrementPendingIO();
            DWORD nRet = 0;

            if (context->IOOperation == IO_OPERATION::IoRead) {
                nRet = WSARecv(context->Socket_->Handle().value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context->Overlapped), NULL);
            }
            else {
                nRet = WSASend(context->Socket_->Handle().value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context->Overlapped), NULL);
            }
            auto lasterr = WSAGetLastError();
            if (nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                if (auto handler(context->getCompletionHandler()); handler) {
                    context->Context_->DecrementPendingIO();
                    handler(TranslateError(&lasterr), 0);
                }
            }
        }
    }

    void continue_connect(bool success, Win_IO_Context *context)
    {
        if (auto h = context->getCompletionHandler(); h) {
            if (success && ::setsockopt(context->Socket_->Handle().value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
                h(StatusCode::SC_SUCCESS, 0);
            }
            else {
                h(TranslateError(), 0);
            }
        }
    }
    StatusCode BindSocket(SOCKET sock, AddressFamily family)
    {
        if (family == AddressFamily::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            if (::bind(sock, (::sockaddr *)&bindaddr, sizeof(bindaddr)) == SOCKET_ERROR) {
                return TranslateError();
            }
        }
        else {
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            bindaddr.sin6_port = 0;
            if (::bind(sock, (::sockaddr *)&bindaddr, sizeof(bindaddr)) == SOCKET_ERROR) {
                return TranslateError();
            }
        }
        return StatusCode::SC_SUCCESS;
    }
    void connect_async(Socket &c, sockaddr &address, std::function<void(StatusCode)> &&handler)
    {
        c.PlatformSocket_ = PlatformSocket(Family(address));
        auto bindret = BindSocket(c.Handle().Handle().value, Family(address));
        if (bindret != StatusCode::SC_SUCCESS) {
            return handler(bindret);
        }
        if (CreateIoCompletionPort((HANDLE)c.Handle().Handle().value, c.Context_.IOCPHandle, NULL, NULL) == NULL) {
            return handler(TranslateError());
        }
        auto context = c.ReadContext_;
        context->Context_ = &c.Context_;
        context->setCompletionHandler([ihandler(std::move(handler))](StatusCode s, size_t sz) { ihandler(s); });
        context->IOOperation = IO_OPERATION::IoConnect;
        context->Socket_ = &c.PlatformSocket_;
        c.Context_.IncrementPendingIO();

        auto connectres = c.Context_.ConnectEx_(c.Handle().Handle().value, (::sockaddr *)SocketAddr(address), SocketAddrLen(address), 0, 0, 0,
                                                (LPOVERLAPPED)&context->Overlapped);
        if (connectres == TRUE) {
            // connection completed immediatly!
            if (auto h = context->getCompletionHandler(); h) {
                if (::setsockopt(c.Handle().Handle().value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
                    h(StatusCode::SC_SUCCESS, 0);
                }
                else {
                    h(TranslateError(), 0);
                }
            }
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            c.Context_.DecrementPendingIO();
            if (auto h = context->getCompletionHandler(); h) {
                h(TranslateError(), 0);
            }
        }
    }
#else

    void connect_async(Socket &c, sockaddr &address, std::function<void(StatusCode)> &&handler)
    {
        c.PlatformSocket_ = PlatformSocket(Family(address), Blocking_Options::NON_BLOCKING);
        auto ret = ::connect(c.PlatformSocket_.Handle().value, (::sockaddr *)SocketAddr(address), SocketAddrLen(address));
        if (ret == -1) { // will complete some time later
            auto err = errno;
            if (err != EINPROGRESS) { // error with the socket
                return handler(TranslateError(&err));
            }
            else {

                auto context = c.ReadContext_;
                context->Context_ = &c.Context_;
                context->setCompletionHandler([ihandler(std::move(handler))](StatusCode s, size_t sz) { ihandler(s); });
                context->IOOperation = IO_OPERATION::IoConnect;
                context->Socket_ = &c.PlatformSocket_;
                c.Context_.IncrementPendingIO();

                epoll_event ev = {0};
                ev.data.ptr = context;
                ev.events = EPOLLOUT | EPOLLONESHOT;
                if (epoll_ctl(c.Context_.IOCPHandle, EPOLL_CTL_ADD, c.PlatformSocket_.Handle().value, &ev) == -1) {
                    if (auto h = context->getCompletionHandler(); h) {
                        c.Context_.DecrementPendingIO();
                        h(TranslateError(), 0);
                    }
                }
            }
        }
        else { // connection completed
            handler(StatusCode::SC_SUCCESS);
        }
    }

    void continue_connect(bool success, Win_IO_Context *context)
    {
        if (auto h = context->getCompletionHandler(); h) {
            h(StatusCode::SC_SUCCESS, 0);
        }
    }
    void continue_io(bool success, Win_IO_Context *context)
    {
        auto bytestowrite = context->bufferlen - context->transfered_bytes;
        auto count = 0;
        if (context->IOOperation == IO_OPERATION::IoRead) {
            count = ::read(context->Socket_->Handle().value, context->buffer + context->transfered_bytes, bytestowrite);
            if (count <= 0) { // possible error or continue
                if ((errno != EAGAIN && errno != EINTR) || count == 0) {
                    if (auto h(context->getCompletionHandler()); h) {
                        h(TranslateError(), 0);
                    }
                    return; // done get out
                }
                else {
                    count = 0;
                }
            }
        }
        else {
            count = ::write(context->Socket_->Handle().value, context->buffer + context->transfered_bytes, bytestowrite);
            if (count < 0) { // possible error or continue
                if (errno != EAGAIN && errno != EINTR) {
                    if (auto h(context->getCompletionHandler()); h) {
                        h(TranslateError(), 0);
                    }
                    return; // done get out
                }
                else {
                    count = 0;
                }
            }
        }
        context->transfered_bytes += count;
        if (context->transfered_bytes == context->bufferlen) {
            if (auto h(context->getCompletionHandler()); h) {
                h(StatusCode::SC_SUCCESS, context->bufferlen);
            }
            return; // done with this get out!!
        }

        epoll_event ev = {0};
        ev.data.ptr = context;
        ev.events = context->IOOperation == IO_OPERATION::IoRead ? EPOLLIN : EPOLLOUT;
        ev.events |= EPOLLONESHOT;
        context->Context_->IncrementPendingIO();
        if (epoll_ctl(context->Context_->IOCPHandle, EPOLL_CTL_MOD, context->Socket_->Handle().value, &ev) == -1) {
            if (auto h(context->getCompletionHandler()); h) {
                context->Context_->DecrementPendingIO();
                h(TranslateError(), 0);
            }
        }
    }

#endif
} // namespace NET
} // namespace SL
