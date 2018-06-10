
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
    std::shared_ptr<ISocket> ISocket::CreateSocket(Context &c)
    {
        auto sw = std::make_shared<Socket>(c); 
        return std::reinterpret_pointer_cast<ISocket>(sw);
    }
    void completeio(Win_IO_Context &context, StatusCode code, size_t bytes)
    {
        if (auto h(context.getCompletionHandler()); h) {
            context.Context_.DecrementPendingIO();
            h(code, bytes); 
        }
    }
    Socket::Socket(IOData &c) : IOData_(c), ReadContext_(PlatformSocket_, c), WriteContext_(PlatformSocket_, c)
    {
        if (PlatformSocket_.Handle().value != INVALID_SOCKET) {
            Status_ = SocketStatus::OPEN;
        }
        else {
            Status_ = SocketStatus::CLOSED;
        }
    }
    Socket::Socket(IOData &c, PlatformSocket &&p) : Socket(c) { PlatformSocket_ = std::move(p); }
    Socket::Socket(Context &c, PlatformSocket &&p) : Socket(c.getIOData(), std::move(p)) {}
    Socket::Socket(Context &c) : Socket(c.getIOData()) {}
    Socket::Socket(Socket &&sock) : Socket(sock.IOData_, std::move(sock.PlatformSocket_)) {}
    Socket::~Socket() { close(); }
    void Socket::close()
    {
        Status_ = SocketStatus::CLOSED;
        IOData_.DeregisterSocket(this);
        PlatformSocket_.close();
        completeio(ReadContext_, StatusCode::SC_CLOSED, 0);
        completeio(WriteContext_, StatusCode::SC_CLOSED, 0);
    }
    void Socket::recv_async(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        ReadContext_.buffer = buffer;
        ReadContext_.transfered_bytes = 0;
        ReadContext_.bufferlen = buffer_size;
        ReadContext_.setCompletionHandler(std::move(handler));
        ReadContext_.IOOperation = IO_OPERATION::IoRead;

#ifndef _WIN32
        epoll_event ev = {0};
        ev.data.ptr = &ReadContext_;
        ev.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        IOData_.IncrementPendingIO();
        if (epoll_ctl(IOData_.getIOHandle(), EPOLL_CTL_MOD, PlatformSocket_.Handle().value, &ev) == -1) {
            completeio(ReadContext_, TranslateError(), 0);
        }
#else
        ReadContext_.Overlapped = {0};
        continue_io(true, &ReadContext_);
#endif
    }
    void Socket::send_async(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        WriteContext_.buffer = buffer;
        WriteContext_.transfered_bytes = 0;
        WriteContext_.bufferlen = buffer_size;
        WriteContext_.setCompletionHandler(std::move(handler));
        WriteContext_.IOOperation = IO_OPERATION::IoWrite;

#ifndef _WIN32
        epoll_event ev = {0};
        ev.data.ptr = &WriteContext_;
        ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        IOData_.IncrementPendingIO();
        if (epoll_ctl(IOData_.getIOHandle(), EPOLL_CTL_MOD, PlatformSocket_.Handle().value, &ev) == -1) {
            completeio(WriteContext_,  TranslateError(), 0);
        }
#else
        WriteContext_.Overlapped = {0};
        continue_io(true, &WriteContext_);
#endif
    }
#if _WIN32
    void continue_io(bool success, Win_IO_Context *context)
    {
        if (!success) {
            completeio(*context, TranslateError(), 0);
        }
        else if (context->bufferlen == context->transfered_bytes) {
            completeio(*context, StatusCode::SC_SUCCESS, context->transfered_bytes);
        }
        else {
            WSABUF wsabuf = {0};
            auto bytesleft = static_cast<decltype(wsabuf.len)>(context->bufferlen - context->transfered_bytes);
            wsabuf.buf = (char *)context->buffer + context->transfered_bytes;
            wsabuf.len = bytesleft;
            DWORD dwSendNumBytes(0), dwFlags(0);
            context->Context_.IncrementPendingIO();
            DWORD nRet = 0;

            if (context->IOOperation == IO_OPERATION::IoRead) {
                nRet = WSARecv(context->Socket_.Handle().value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context->Overlapped), NULL);
            }
            else {
                nRet = WSASend(context->Socket_.Handle().value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context->Overlapped), NULL);
            }
            auto lasterr = WSAGetLastError();
            if (nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                completeio(*context, TranslateError(&lasterr), 0);
            }
        }
    }
    void continue_connect(bool success, Win_IO_Context *context)
    {
        if (success && ::setsockopt(context->Socket_.Handle().value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
            completeio(*context, StatusCode::SC_SUCCESS, 0);
        }
        else {
            completeio(*context, TranslateError(), 0);
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
    void connect_async(std::shared_ptr<ISocket> &socket, sockaddr &address, std::function<void(StatusCode)> &&handler)
    {
        auto &c = *std::reinterpret_pointer_cast<Socket>(socket);
        c.PlatformSocket_ = PlatformSocket(Family(address), Blocking_Options::NON_BLOCKING);
        c.Status_ = SocketStatus::CONNECTING;
       
        auto bindret = BindSocket(c.Handle().Handle().value, Family(address));
        if (bindret != StatusCode::SC_SUCCESS) {
            return handler(bindret);
        }
        if (CreateIoCompletionPort((HANDLE)c.Handle().Handle().value, c.IOData_.getIOHandle(), NULL, NULL) == NULL) {
            return handler(TranslateError());
        }
        auto &context = c.ReadContext_;
        context.setCompletionHandler([ihandler(std::move(handler))](StatusCode s, size_t) { ihandler(s); });
        context.IOOperation = IO_OPERATION::IoConnect;
        c.IOData_.IncrementPendingIO();

        auto connectres = c.IOData_.ConnectEx_(c.Handle().Handle().value, (::sockaddr *)SocketAddr(address), SocketAddrLen(address), 0, 0, 0,
                                               (LPOVERLAPPED)&context.Overlapped);
        if (connectres == TRUE) {
            // connection completed immediatly!
            if (::setsockopt(c.Handle().Handle().value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
                completeio(context, StatusCode::SC_SUCCESS, 0);
            }
            else {
                completeio(context, TranslateError(), 0);
            }
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            completeio(context, TranslateError(), 0);
        }
    }
#else

    void connect_async(std::shared_ptr<ISocket> &socket, sockaddr &address, std::function<void(StatusCode)> &&handler)
    {
        auto &c = *std::reinterpret_pointer_cast<Socket>(socket);
        c.PlatformSocket_ = PlatformSocket(Family(address), Blocking_Options::NON_BLOCKING);
        c.Status_ = SocketStatus::CONNECTING;
        c.IOData_.RegisterSocket(std::reinterpret_pointer_cast<Socket>(socket));
        auto ret = ::connect(c.PlatformSocket_.Handle().value, (::sockaddr *)SocketAddr(address), SocketAddrLen(address));
        if (ret == -1) { // will complete some time later
            auto err = errno;
            if (err != EINPROGRESS) { // error with the socket
                return handler(TranslateError(&err));
            }
            else {

                auto &context = c.ReadContext_;
                context.setCompletionHandler([ihandler(std::move(handler))](StatusCode s, size_t sz) { ihandler(s); });
                context.IOOperation = IO_OPERATION::IoConnect;
                c.IOData_.IncrementPendingIO();

                epoll_event ev = {0};
                ev.data.ptr = socket.get();
                ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
                if (epoll_ctl(c.IOData_.getIOHandle(), EPOLL_CTL_ADD, c.PlatformSocket_.Handle().value, &ev) == -1) {
                    completeio(context, TranslateError(), 0);
                }
            }
        }
        else { // connection completed
            handler(StatusCode::SC_SUCCESS);
        }
    }

    void continue_connect(bool success, Win_IO_Context *context)
    {
        if (success) {
            completeio(*context, StatusCode::SC_SUCCESS, 0); 
        }
        else {
            completeio(*context, StatusCode::SC_CLOSED, 0); 
        }
    }
    void continue_io(bool success, Win_IO_Context *context, const std::shared_ptr<Socket>& socket)
    {
        if (!success) {  
            completeio(*context, StatusCode::SC_CLOSED, 0); 
        }
        auto bytestowrite = context->bufferlen - context->transfered_bytes;
        auto count = 0;
        if (context->IOOperation == IO_OPERATION::IoRead) {
            count = ::read(context->Socket_.Handle().value, context->buffer + context->transfered_bytes, bytestowrite);
            if (count <= 0) { // possible error or continue
                if ((errno != EAGAIN && errno != EINTR) || count == 0) {
                    return completeio(*context,TranslateError(), 0); 
                }
                else {
                    count = 0;
                }
            }
        }
        else {
            count = ::write(context->Socket_.Handle().value, context->buffer + context->transfered_bytes, bytestowrite);
            if (count < 0) { // possible error or continue
                if (errno != EAGAIN && errno != EINTR) {
                    return completeio(*context,TranslateError(), 0); 
                }
                else {
                    count = 0;
                }
            }
        }
        context->transfered_bytes += count;
        if (context->transfered_bytes == context->bufferlen) {
            return completeio(*context, StatusCode::SC_SUCCESS, context->bufferlen);  
        }

        epoll_event ev = {0};
        ev.data.ptr = socket.get();
        ev.events = context->IOOperation == IO_OPERATION::IoRead ? EPOLLIN : EPOLLOUT;
        ev.events |= EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        context->Context_.IncrementPendingIO();
        if (epoll_ctl(context->Context_.getIOHandle(), EPOLL_CTL_MOD, context->Socket_.Handle().value, &ev) == -1) {
            return completeio(*context,TranslateError(), 0); 
        }
    }

#endif
} // namespace NET
} // namespace SL
