#pragma once
#include "PlatformSocket.h"

namespace SL::NET {

template <class CONTEXTTYPE = Context> class AsyncPlatformSocket : public PlatformSocket {
    CONTEXTTYPE &Context_;

    template <class IOCONTEXTTYPE> StatusCode continue_partial_send_async(const IOCONTEXTTYPE &context)
    {
#if _WIN32
        WSABUF wsabuf;
        wsabuf.buf = (char *)context.buffer;
        wsabuf.len = static_cast<decltype(wsabuf.len)>(context.getRemainingBytes());
        DWORD dwSendNumBytes(0), dwFlags(0);
        DWORD nRet = WSASend(Handle.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context.Overlapped), NULL);
        if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
            completeio(context, Context_, TranslateError(&lasterr));
        }
#else
        context.PostDeferredWriteIO(Handle.value);
#endif
    }

    template <class IOCONTEXTTYPE> StatusCode continue_partial_recv_async(const IOCONTEXTTYPE &context)
    {
#if _WIN32
        WSABUF wsabuf;
        wsabuf.buf = (char *)context.buffer;
        wsabuf.len = static_cast<decltype(wsabuf.len)>(context.getRemainingBytes());
        DWORD dwSendNumBytes(0), dwFlags(0);
        DWORD nRet = WSARecv(PlatformSocket_.Handle().value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context.Overlapped), NULL);
        if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
            completeio(context, Context_, TranslateError(&lasterr));
        }
#else
        context.PostDeferredReadIO(Handle.value);
#endif
    }
    template <class IOCONTEXTTYPE> StatusCode continue_recv_async(const IOCONTEXTTYPE &context)
    {

#if _WIN32
        WSABUF wsabuf;
        wsabuf.buf = (char *)context.buffer;
        wsabuf.len = static_cast<decltype(wsabuf.len)>(context.getRemainingBytes());
        DWORD dwSendNumBytes(0), dwFlags(0);
        DWORD nRet = WSARecv(Handle.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context.Overlapped), NULL);
        if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
            completeio(context, Context_, TranslateError(&lasterr));
        }
#else

        epoll_event ev = {0};
        ev.data.fd = handle.value;
        ev.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        if (epoll_ctl(context.getIOHandle(), EPOLL_CTL_MOD, handle.value, &ev) == -1) {
            completeio(context, Context_, TranslateError());
        }
#endif
    }
    template <class IOCONTEXTTYPE> StatusCode continue_send_async(const IOCONTEXTTYPE &context)
    {
#if _WIN32
        WSABUF wsabuf;
        wsabuf.buf = (char *)context.buffer;
        wsabuf.len = static_cast<decltype(wsabuf.len)>(context.getRemainingBytes());
        DWORD dwSendNumBytes(0), dwFlags(0);
        DWORD nRet = WSASend(Handle.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context.Overlapped), NULL);
        if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
            completeio(context, Context_, TranslateError(&lasterr));
        }
#else

        epoll_event ev = {0};
        ev.data.fd = handle.value;
        ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        if (epoll_ctl(context.getIOHandle(), EPOLL_CTL_MOD, handle.value, &ev) == -1) {
            completeio(context, Context_, TranslateError());
        }
#endif
    }

  public:
    AsyncPlatformSocket(const CONTEXTTYPE &c) : Context_(c) {}
    AsyncPlatformSocket(AsyncPlatformSocket &&p) : Handle_(std::move(p.Handle_)) { p.Handle_.value = INVALID_SOCKET; }
    ~AsyncPlatformSocket() { Context_.DeregisterSocket(Handle); }
    CONTEXTTYPE &getContext() { return Context_; }
    template <class SOCKEtHANDLERTYPE> StatusCode send(unsigned char *buffer, int buffer_size, const SOCKEtHANDLERTYPE &handler)
    {
        static counter = 0;
        auto[statuscode, count] = send(buffer, len);
        if (statuscode == StatusCode::SC_SUCCESS) {
            if (count == buffer_size) {
                if (counter++ < 8) {
                    handler(statuscode);
                }
                else {
                    counter = 0;
                    auto &context = Context_.getWriteContext(Handle.value);
                    setup(context, Context_, IO_OPERATION::IoWrite, buffer_size, buffer, handler);
                    context.PostDeferredWriteIO(Handle.value);
                }
            }
            else {
                counter = 0;
                auto &context = Context_.getWriteContext(Handle.value);
                setup(context, Context_, IO_OPERATION::IoWrite, buffer_size, buffer, handler);
                context.setRemainingBytes(context.getRemainingBytes() - count);
                context.buffer += count;
                continue_partial_send_async(context);
            }
        }
        else if (statuscode == StatusCode::SC_EWOULDBLOCK) {
            auto &context = Context_.getWriteContext(Handle.value);
            setup(context, Context_, IO_OPERATION::IoWrite, buffer_size, buffer, handler);
            continue_send_async(context);
        }
        else {
            handler(TranslateError());
        }
    }

    template <class SOCKEtHANDLERTYPE> StatusCode recv(unsigned char *buffer, int buffer_size, const SOCKEtHANDLERTYPE &handler)
    {
        static counter = 0;
        auto[statuscode, count] = recv(buffer, len);
        if (statuscode == StatusCode::SC_SUCCESS) {
            if (count == buffer_size) {
                if (counter++ < 8) {
                    handler(statuscode);
                }
                else {
                    counter = 0;
                    auto &context = Context_.getWriteContext(Handle.value);
                    setup(context, Context_, IO_OPERATION::IoWrite, buffer_size, buffer, handler);
                    context.PostDeferredReadIO(Handle.value);
                }
            }
            else {
                counter = 0;
                auto &context = Context_.getReadContext(Handle.value);
                context.setRemainingBytes(context.getRemainingBytes() - count);
                context.buffer += count;
                continue_partial_recv_async(context);
            }
        }
        else if (statuscode == StatusCode::SC_EWOULDBLOCK) {
            continue_recv_async(Context_.getReadContext(Handle.value));
        }
        else {
            handler(TranslateError());
        }
    }
    static std::tuple<StatusCode, AsyncPlatformSocket> Create(const AddressFamily &family, const CONTEXTTYPE &context)
    {
        int typ = SOCK_STREAM;
        AsyncSocketHandle handle(context);
        auto errcode = StatusCode::SC_SUCCESS;
#if !_WIN32
        typ |= SOCK_NONBLOCK;
#endif
        if (family == AddressFamily::IPV4) {
            handle.value = socket(AF_INET, typ, 0);
        }
        else {
            handle.value = socket(AF_INET6, typ, 0);
        }
        if (handle.value == INVALID_SOCKET) {
            return std::tuple(TranslateError(), handle);
        }
#if _WIN32
        [[maybe_unused]] auto e = handle.setsockopt(BLOCKINGTag{}, Blocking_Options::NON_BLOCKING);
        if (CreateIoCompletionPort((HANDLE)handle.value, context.getIOHandle(), handle.value, NULL) == NULL) {
            return std::tuple(TranslateError(), handle);
        }
#else
        epoll_event ev = {0};
        ev.data.fd = hhandle;
        ev.events = EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        if (epoll_ctl(context.getIOHandle(), EPOLL_CTL_ADD, hhandle, &ev) == -1) {
            return std::tuple(TranslateError(), handle);
        }
#endif
        return std::tuple(handle, errcode);
    }

    template <class SOCKEtHANDLERTYPE>
    static StatusCode connect(AsyncPlatformSocket &socket, SocketAddress &address, const SOCKEtHANDLERTYPE &handler)
    {
        auto[statucode, sock] = Create(Family(address), socket.getContext());
        if (statucode != StatusCode::SC_SUCCESS) {
            return statucode;
        }
        auto handle = sock.Handle_.value;
        socket.close();
        socket.Handle_ = handle;
#if _WIN32
        if (family == AddressFamily::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            if (::bind(handle, (::sockaddr *)&bindaddr, sizeof(bindaddr)) == SOCKET_ERROR) {
                return TranslateError();
            }
        }
        else {
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            bindaddr.sin6_port = 0;
            if (::bind(handle, (::sockaddr *)&bindaddr, sizeof(bindaddr)) == SOCKET_ERROR) {
                return TranslateError();
            }
        }
        auto &context = sock.Context_.getWriteContext(sock.Handle_);
        context.setCompletionHandler(handler);
        context.setEvent(IO_OPERATION::IoConnect);
        sock.Context_.IncrementPendingIO();

        auto connectres =
            sock.Context_.ConnectEx_(handle, (::sockaddr *)SocketAddr(address), SocketAddrLen(address), 0, 0, 0, (LPOVERLAPPED)&context.Overlapped);
        if (connectres == TRUE) {
            // connection completed immediatly!
            if (::setsockopt(handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
                completeio(context, socket.Context_, StatusCode::SC_SUCCESS);
            }
            else {
                completeio(context, socket.Context_, TranslateError());
            }
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            completeio(context, socket.Context_, TranslateError());
        }

#else
        auto ret = ::connect(handle, (::sockaddr *)SocketAddr(address), SocketAddrLen(address));
        if (ret == -1) { // will complete some time later
            auto err = errno;
            if (err != EINPROGRESS) { // error with the socket
                return handler(TranslateError(&err));
            }
            else {
                auto &context = socket.Context_.getWriteContext(handle);
                context.setCompletionHandler(handler);
                context.setEvent(IO_OPERATION::IoConnect);
                socket.Context_.IncrementPendingIO();
                epoll_event ev = {0};
                ev.data.fd = handle;
                ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
                if (epoll_ctl(socket.Context_.getIOHandle(), EPOLL_CTL_MOD, handle, &ev) == -1) {
                    return completeio(context, socket.Context_, TranslateError());
                }
            }
        }
        else { // connection completed
            return std::tuple(StatusCode::SC_SUCCESS, std::move(sock));
        }
#endif
    }
};
typedef SL::NET::AsyncPlatformSocket<> AsyncSocket;

} // namespace SL::NET