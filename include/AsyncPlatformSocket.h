#pragma once
#include "Context.h"
#include "PlatformSocket.h"
namespace SL::NET {

class AsyncPlatformSocket : public PlatformSocket {
    friend class AsyncPlatformAcceptor;
    friend Context;
    Context &Context_;

    void continue_partial_send_async(RW_Context &rwcontext)
    {
#if _WIN32
        WSABUF wsabuf;
        wsabuf.buf = (char *)rwcontext.buffer;
        wsabuf.len = static_cast<decltype(wsabuf.len)>(rwcontext.getRemainingBytes());
        DWORD dwSendNumBytes(0), dwFlags(0);
        DWORD nRet = WSASend(Handle_.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(rwcontext.Overlapped), NULL);
        if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
            completeio(rwcontext, Context_, TranslateError(&lasterr));
        }
#else
        context.PostDeferredWriteIO(Handle_.value);
#endif
    }
    void continue_send_async(bool success, RW_Context &rwcontext)
    {
        auto[statuscode, count] = PlatformSocket::send(rwcontext.buffer, rwcontext.getRemainingBytes());
        if (statuscode == StatusCode::SC_SUCCESS) {
            if (count == rwcontext.getRemainingBytes()) {
                completeio(rwcontext, Context_, StatusCode::SC_SUCCESS);
            }
            else {
                rwcontext.setRemainingBytes(rwcontext.getRemainingBytes() - count);
                rwcontext.buffer += count;
                continue_partial_send_async(rwcontext);
            }
        }
        else if (statuscode == StatusCode::SC_EWOULDBLOCK) {
            continue_send_async(rwcontext);
        }
        else {
            completeio(rwcontext, Context_, TranslateError());
        }
    }
    void continue_partial_recv_async(RW_Context &rwcontext)
    {
#if _WIN32
        WSABUF wsabuf;
        wsabuf.buf = (char *)rwcontext.buffer;
        wsabuf.len = static_cast<decltype(wsabuf.len)>(rwcontext.getRemainingBytes());
        DWORD dwSendNumBytes(0), dwFlags(0);
        DWORD nRet = WSARecv(Handle_.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(rwcontext.Overlapped), NULL);
        if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
            completeio(rwcontext, Context_, TranslateError(&lasterr));
        }
#else
        context.PostDeferredReadIO(Handle_.value);
#endif
    }
    void continue_recv_async(bool success, RW_Context &rwcontext)
    {
        auto[statuscode, count] = PlatformSocket::recv(rwcontext.buffer, rwcontext.getRemainingBytes());
        if (statuscode == StatusCode::SC_SUCCESS) {
            if (count == rwcontext.getRemainingBytes()) {
                completeio(rwcontext, Context_, StatusCode::SC_SUCCESS);
            }
            else {
                rwcontext.setRemainingBytes(rwcontext.getRemainingBytes() - count);
                rwcontext.buffer += count;
                continue_partial_recv_async(rwcontext);
            }
        }
        else if (statuscode == StatusCode::SC_EWOULDBLOCK) {
            continue_recv_async(rwcontext);
        }
        else {
            completeio(rwcontext, Context_, TranslateError());
        }
    }

    void continue_recv_async(RW_Context &rwcontext)
    {

#if _WIN32
        WSABUF wsabuf;
        wsabuf.buf = (char *)rwcontext.buffer;
        wsabuf.len = static_cast<decltype(wsabuf.len)>(rwcontext.getRemainingBytes());
        DWORD dwSendNumBytes(0), dwFlags(0);
        DWORD nRet = WSARecv(Handle_.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(rwcontext.Overlapped), NULL);
        if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
            completeio(rwcontext, Context_, TranslateError(&lasterr));
        }
#else
        epoll_event ev = {0};
        ev.data.fd = Handle_.value;
        ev.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        if (epoll_ctl(context.getIOHandle(), EPOLL_CTL_MOD, Handle_.value, &ev) == -1) {
            completeio(context, Context_, TranslateError());
        }
#endif
    }
    void continue_send_async(RW_Context &rwcontext)
    {
#if _WIN32
        WSABUF wsabuf;
        wsabuf.buf = (char *)rwcontext.buffer;
        wsabuf.len = static_cast<decltype(wsabuf.len)>(rwcontext.getRemainingBytes());
        DWORD dwSendNumBytes(0), dwFlags(0);
        DWORD nRet = WSASend(Handle_.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(rwcontext.Overlapped), NULL);
        if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
            completeio(rwcontext, Context_, TranslateError(&lasterr));
        }
#else
        epoll_event ev = {0};
        ev.data.fd = Handle_.value;
        ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        if (epoll_ctl(context.getIOHandle(), EPOLL_CTL_MOD, Handle_.value, &ev) == -1) {
            completeio(context, Context_, TranslateError());
        }
#endif
    }

  public:
    AsyncPlatformSocket(Context &c) : Context_(c) {}
    AsyncPlatformSocket(Context &c, SocketHandle h) : PlatformSocket(h), Context_(c) {}
    AsyncPlatformSocket(AsyncPlatformSocket &&p) : Context_(p.Context_)
    {
        Handle_.value = p.Handle_.value;
        p.Handle_.value = INVALID_SOCKET;
    }
    ~AsyncPlatformSocket() { Context_.DeregisterSocket(Handle_); }
    Context &getContext() { return Context_; }
    const Context &getContext() const { return Context_; }
    template <class SOCKETHANDLERTYPE> void send(unsigned char *buffer, int buffer_size, const SOCKETHANDLERTYPE &handler)
    {
        static int counter = 0;
        auto[statuscode, count] = PlatformSocket::send(buffer, buffer_size);
        if (statuscode == StatusCode::SC_SUCCESS) {
            if (count == buffer_size) {
                if (counter++ < 8) {
                    handler(statuscode);
                }
                else {
                    counter = 0;
                    auto &context = Context_.getWriteContext(Handle_.value);
                    setup(context, Context_, IO_OPERATION::IoWrite, buffer_size, buffer, handler);
                    Context_.PostDeferredWriteIO(Handle_.value);
                }
            }
            else {
                counter = 0;
                auto &context = Context_.getWriteContext(Handle_.value);
                setup(context, Context_, IO_OPERATION::IoWrite, buffer_size, buffer, handler);
                context.setRemainingBytes(context.getRemainingBytes() - count);
                context.buffer += count;
                continue_partial_send_async(context);
            }
        }
        else if (statuscode == StatusCode::SC_EWOULDBLOCK) {
            auto &context = Context_.getWriteContext(Handle_.value);
            setup(context, Context_, IO_OPERATION::IoWrite, buffer_size, buffer, handler);
            continue_send_async(context);
        }
        else {
            handler(TranslateError());
        }
    }

    template <class SOCKETHANDLERTYPE> void recv(unsigned char *buffer, int buffer_size, const SOCKETHANDLERTYPE &handler)
    {
        static int counter = 0;
        auto[statuscode, count] = PlatformSocket::recv(buffer, buffer_size);
        if (statuscode == StatusCode::SC_SUCCESS) {
            if (count == buffer_size) {
                if (counter++ < 8) {
                    handler(statuscode);
                }
                else {
                    counter = 0;
                    auto &context = Context_.getWriteContext(Handle_.value);
                    setup(context, Context_, IO_OPERATION::IoWrite, buffer_size, buffer, handler);
                    Context_.PostDeferredReadIO(Handle_.value);
                }
            }
            else {
                counter = 0;
                auto &context = Context_.getReadContext(Handle_.value);
                context.setRemainingBytes(context.getRemainingBytes() - count);
                context.buffer += count;
                continue_partial_recv_async(context);
            }
        }
        else if (statuscode == StatusCode::SC_EWOULDBLOCK) {
            continue_recv_async(Context_.getReadContext(Handle_.value));
        }
        else {
            handler(TranslateError());
        }
    }
    static std::tuple<StatusCode, AsyncPlatformSocket> Create(const AddressFamily &family, Context &context)
    {
        int typ = SOCK_STREAM;
        AsyncPlatformSocket handle(context);
        auto errcode = StatusCode::SC_SUCCESS;
#if !_WIN32
        typ |= SOCK_NONBLOCK;
#endif
        if (family == AddressFamily::IPV4) {
            handle.Handle_.value = ::socket(AF_INET, typ, 0);
        }
        else {
            handle.Handle_.value = ::socket(AF_INET6, typ, 0);
        }
        if (handle.Handle_.value == INVALID_SOCKET) {
            return std::tuple(TranslateError(), std::move(handle));
        }
#if _WIN32
        [[maybe_unused]] auto e = handle.setsockopt(BLOCKINGTag{}, Blocking_Options::NON_BLOCKING);
        if (CreateIoCompletionPort((HANDLE)handle.Handle_.value, context.getIOHandle(), handle.Handle_.value, NULL) == NULL) {
            return std::tuple(TranslateError(), std::move(handle));
        }
#else
        epoll_event ev = {0};
        ev.data.fd = hhandle;
        ev.events = EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        if (epoll_ctl(context.getIOHandle(), EPOLL_CTL_ADD, hhandle, &ev) == -1) {
            return std::tuple(TranslateError(), handle);
        }
#endif
        return std::tuple(errcode, std::move(handle));
    }

    template <class SOCKETHANDLERTYPE> static void connect(AsyncPlatformSocket &socket, SocketAddress &address, const SOCKETHANDLERTYPE &handler)
    {
        socket.close();
#if _WIN32

        if (address.getFamily() == AddressFamily::IPV4) {
            socket.Handle_.value = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            if (::bind(socket.Handle_.value, (::sockaddr *)&bindaddr, sizeof(bindaddr)) == SOCKET_ERROR) {
                return handler(TranslateError());
            }
        }
        else {
            socket.Handle_.value = WSASocketW(AF_INET6, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            bindaddr.sin6_port = 0;
            if (::bind(socket.Handle_.value, (::sockaddr *)&bindaddr, sizeof(bindaddr)) == SOCKET_ERROR) {
                return handler(TranslateError());
            }
        }
        u_long iMode = 1;
        ioctlsocket(socket.Handle_.value, FIONBIO, &iMode);
        socket.Handle_.value = socket.Handle_.value;

        auto &context = socket.Context_.getWriteContext(socket.Handle_);
        context.setCompletionHandler(handler);
        context.setEvent(IO_OPERATION::IoConnect);
        socket.Context_.IncrementPendingIO();

        auto connectres = socket.Context_.ConnectEx_(socket.Handle_.value, address.getSocketAddr(), address.getSocketAddrLen(), 0, 0, 0,
                                                     (LPOVERLAPPED)&context.Overlapped);
        if (connectres == TRUE) {
            // connection completed immediatly!
            if (::setsockopt(socket.Handle_.value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
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

template <class CONTEXTTYPE>
void HandleIoEvent(bool bSuccess, DWORD numberofbytestransfered, RW_Context *overlapped, CONTEXTTYPE &iodata, SocketHandle handle)
{
    SL::NET::AsyncPlatformSocket sock(iodata, handle);
    switch (auto eventyype = overlapped->getEvent()) {
    case IO_OPERATION::IoRead:
        overlapped->setRemainingBytes(overlapped->getRemainingBytes() - numberofbytestransfered);
        overlapped->buffer += numberofbytestransfered; // advance the start of the buffer
        if (numberofbytestransfered == 0 && bSuccess) {
            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
        }
        sock.continue_recv_async(bSuccess, *overlapped);

        break;
    case IO_OPERATION::IoWrite:
        overlapped->setRemainingBytes(overlapped->getRemainingBytes() - numberofbytestransfered);
        overlapped->buffer += numberofbytestransfered; // advance the start of the buffer
        if (numberofbytestransfered == 0 && bSuccess) {
            bSuccess = WSAGetLastError() == WSA_IO_PENDING;
        }
        sock.continue_send_async(bSuccess, *overlapped);
        break;
    case IO_OPERATION::IoConnect:
        sock.continue_connect_async(bSuccess, *overlapped);
        break;
    case IO_OPERATION::IoAccept:
        continue_accept(bSuccess, *overlapped, iodata);
        break;
    default:
        break;
    }
}
typedef SL::NET::AsyncPlatformSocket AsyncSocket;

} // namespace SL::NET