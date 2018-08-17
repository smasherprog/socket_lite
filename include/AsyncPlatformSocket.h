#pragma once
#include "Context.h"
#include "Internal.h"
#include "PlatformSocket.h"

namespace SL::NET
{

class AsyncPlatformSocket : public PlatformSocket
{
    friend class AsyncPlatformAcceptor;
#if _WIN32
    static INTERNAL::ConnectExCreator ConnectExCreator_;
#endif

    Context &Context_;

public:
AsyncPlatformSocket(Context &c) noexcept :
    Context_(c) {}
AsyncPlatformSocket(Context &c, SocketHandle h) noexcept :
    PlatformSocket(h), Context_(c) {}
AsyncPlatformSocket(AsyncPlatformSocket &&p) noexcept :
    AsyncPlatformSocket(p.Context_) {
        Handle_.value = p.Handle_.value;
        p.Handle_.value = INVALID_SOCKET;
    }

    ~AsyncPlatformSocket() noexcept { Context_.DeregisterSocket(Handle_); }
    Context &getContext() noexcept { return Context_; }
    const Context &getContext() const noexcept {
        return Context_;
    }
    template <class SOCKETHANDLERTYPE> void send(unsigned char *buffer, int buffer_size, const SOCKETHANDLERTYPE &handler) noexcept {
        static int counter = 0;
        auto count = INTERNAL::Send(Handle_, buffer, buffer_size);
        if (count == buffer_size) {
            if (counter++ < 8) {
                handler(StatusCode::SC_SUCCESS);
            } else {
                counter = 0;
                auto &context = Context_.getWriteContext(Handle_);
                INTERNAL::setup(context, Context_.PendingIO, IO_OPERATION::IoWrite, buffer_size, buffer, handler);
#if _WIN32

                PostQueuedCompletionStatus(Context_.getIOHandle(), count, Handle_.value, &(context.Overlapped));
#else
                epoll_event ev = {0};
                ev.data.fd = Handle_.value;
                ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
                if (epoll_ctl(Context_.getIOHandle(), EPOLL_CTL_MOD, Handle_.value, &ev) == -1) {
                    INTERNAL::completeio(context, Context_.PendingIO, TranslateError());
                }
#endif
            }
        } else if (count > 0) {
            auto &context = Context_.getWriteContext(Handle_);
            INTERNAL::setup(context, Context_.PendingIO, IO_OPERATION::IoWrite, buffer_size - count, buffer + count, handler);
#if _WIN32
            WSABUF wsabuf;
            wsabuf.buf = (char *)context.buffer;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(context.getRemainingBytes());
            DWORD dwSendNumBytes(0), dwFlags(0);
            DWORD nRet = WSASend(Handle_.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context.Overlapped), NULL);
            if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                INTERNAL::completeio(context, Context_.PendingIO, TranslateError(&lasterr));
            }
#else
            epoll_event ev = {0};
            ev.data.fd = Handle_.value;
            ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
            if (epoll_ctl(Context_.getIOHandle(), EPOLL_CTL_MOD, Handle_.value, &ev) == -1) {
                INTERNAL::completeio(context, Context_.PendingIO, TranslateError());
            }
#endif
        } else if (INTERNAL::wouldBlock()) {
            auto &context = Context_.getWriteContext(Handle_);
            INTERNAL::setup(context, Context_.PendingIO, IO_OPERATION::IoWrite, buffer_size, buffer, handler);
#if _WIN32
            WSABUF wsabuf;
            wsabuf.buf = (char *)context.buffer;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(context.getRemainingBytes());
            DWORD dwSendNumBytes(0), dwFlags(0);
            DWORD nRet = WSASend(Handle_.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context.Overlapped), NULL);
            if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                INTERNAL::completeio(context, Context_.PendingIO, TranslateError(&lasterr));
            }
#else
            epoll_event ev = {0};
            ev.data.fd = Handle_.value;
            ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
            if (epoll_ctl(Context_.getIOHandle(), EPOLL_CTL_MOD, Handle_.value, &ev) == -1) {
                INTERNAL::completeio(context, Context_.PendingIO, TranslateError());
            }
#endif
        } else {
            handler(TranslateError());
        }
    }

    template <class SOCKETHANDLERTYPE> void recv(unsigned char *buffer, int buffer_size, const SOCKETHANDLERTYPE &handler) noexcept {
        static int counter = 0;
        auto count = INTERNAL::Recv(Handle_, buffer, buffer_size);
        if (count == buffer_size) {
            if (counter++ < 8) {
                handler(StatusCode::SC_SUCCESS);
            } else {
                counter = 0;
                auto &context = Context_.getReadContext(Handle_);
                INTERNAL::setup(context, Context_.PendingIO, IO_OPERATION::IoRead, buffer_size, buffer, handler);
#if _WIN32

                PostQueuedCompletionStatus(Context_.getIOHandle(), count, Handle_.value, &(context.Overlapped));
#else
                epoll_event ev = {0};
                ev.data.fd = Handle_.value;
                ev.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
                if (epoll_ctl(Context_.getIOHandle(), EPOLL_CTL_MOD, Handle_.value, &ev) == -1) {
                    INTERNAL::completeio(context, Context_.PendingIO, TranslateError());
                }
#endif
            }
        } else if (count > 0) {
            auto &context = Context_.getReadContext(Handle_);
            INTERNAL::setup(context, Context_.PendingIO, IO_OPERATION::IoRead, buffer_size - count, buffer + count, handler);
#if _WIN32
            WSABUF wsabuf;
            wsabuf.buf = (char *)context.buffer;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(context.getRemainingBytes());
            DWORD dwSendNumBytes(0), dwFlags(0);
            DWORD nRet = WSARecv(Handle_.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context.Overlapped), NULL);
            if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                INTERNAL::completeio(context, Context_.PendingIO, TranslateError(&lasterr));
            }
#else
            epoll_event ev = {0};
            ev.data.fd = Handle_.value;
            ev.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
            if (epoll_ctl(Context_.getIOHandle(), EPOLL_CTL_MOD, Handle_.value, &ev) == -1) {
                INTERNAL::completeio(context, Context_.PendingIO, TranslateError());
            }
#endif
        } else if (INTERNAL::wouldBlock()) {
            auto &context = Context_.getReadContext(Handle_);
            INTERNAL::setup(context, Context_.PendingIO, IO_OPERATION::IoRead, buffer_size, buffer, handler);
#if _WIN32
            WSABUF wsabuf;
            wsabuf.buf = (char *)context.buffer;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(context.getRemainingBytes());
            DWORD dwSendNumBytes(0), dwFlags(0);
            DWORD nRet = WSARecv(Handle_.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context.Overlapped), NULL);
            if (auto lasterr = WSAGetLastError(); nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                INTERNAL::completeio(context, Context_.PendingIO, TranslateError(&lasterr));
            }
#else
            epoll_event ev = {0};
            ev.data.fd = Handle_.value;
            ev.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
            if (epoll_ctl(Context_.getIOHandle(), EPOLL_CTL_MOD, Handle_.value, &ev) == -1) {
                INTERNAL::completeio(context, Context_.PendingIO, TranslateError());
            }
#endif
        } else {
            handler(TranslateError());
        }
    }
    static std::tuple<StatusCode, AsyncPlatformSocket> Create(const AddressFamily &family, Context &context) {
        int typ = SOCK_STREAM;
        AsyncPlatformSocket handle(context);
        auto errcode = StatusCode::SC_SUCCESS;
#if !_WIN32
        typ |= SOCK_NONBLOCK;
#endif
        if (family == AddressFamily::IPV4) {
            handle.Handle_.value = ::socket(AF_INET, typ, 0);
        } else {
            handle.Handle_.value = ::socket(AF_INET6, typ, 0);
        }
        if (handle.Handle_.value == INVALID_SOCKET) {
            return std::tuple(TranslateError(), std::move(handle));
        }
#if _WIN32
        [[maybe_unused]] auto e = handle.setsockopt(BLOCKINGTag {}, Blocking_Options::NON_BLOCKING);
        if (CreateIoCompletionPort((HANDLE)handle.Handle_.value, context.getIOHandle(), handle.Handle_.value, NULL) == NULL) {
            return std::tuple(TranslateError(), std::move(handle));
        }
#else
        epoll_event ev = {0};
        ev.data.fd = handle.Handle_.value;
        ev.events = EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        if (epoll_ctl(context.getIOHandle(), EPOLL_CTL_ADD, handle.Handle_.value, &ev) == -1) {
            return std::tuple(TranslateError(), std::move(handle));
        }
#endif
        return std::tuple(errcode, std::move(handle));
    }

    template <class SOCKETHANDLERTYPE> static void connect(AsyncPlatformSocket &socket, SocketAddress &address, const SOCKETHANDLERTYPE &handler) {
        socket.close();
#if _WIN32
        socket.Handle_.value = ::socket(AF_INET, SOCK_STREAM, 0);
        if (address.getFamily() == AddressFamily::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            if (::bind(socket.Handle_.value, (::sockaddr *)&bindaddr, sizeof(bindaddr)) == SOCKET_ERROR) {
                return handler(TranslateError());
            }
        } else {
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
        if (CreateIoCompletionPort((HANDLE)socket.Handle_.value, socket.Context_.getIOHandle(), socket.Handle_.value, NULL) == NULL) {
            return handler(TranslateError());
        }

        auto &context = socket.Context_.getWriteContext(socket.Handle_);
        INTERNAL::setup(context, socket.Context_.PendingIO, IO_OPERATION::IoConnect, 0, nullptr,
        [handler(std::move(handler)), sockhandle = socket.Handle_.value](StatusCode sc) {
            if (sc == StatusCode::SC_SUCCESS) {
                if (::setsockopt(sockhandle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
                    handler(sc);
                } else {
                    handler(TranslateError());
                }
            } else {
                handler(sc);
            }
        });
        DWORD sendbytes(0);
        auto connectres = ConnectExCreator_.ConnectEx_(socket.Handle_.value, address.getSocketAddr(), address.getSocketAddrLen(), 0, 0, &sendbytes,
                          (LPOVERLAPPED)&context.Overlapped);
        if (connectres == TRUE) {
            // connection completed immediatly!
            if (::setsockopt(socket.Handle_.value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
                INTERNAL::completeio(context, socket.Context_.PendingIO, StatusCode::SC_SUCCESS);
            } else {
                INTERNAL::completeio(context, socket.Context_.PendingIO, TranslateError());
            }
        } else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            INTERNAL::completeio(context, socket.Context_.PendingIO, TranslateError());
        }

#else
        socket.Handle_.value = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
        auto ret = ::connect(socket.Handle_.value,  address.getSocketAddr(), address.getSocketAddrLen());
        if (ret == -1) { // will complete some time later
            auto err = errno;
            if (err != EINPROGRESS) { // error with the socket
                return handler(TranslateError(&err));
            } else {
                auto &context = socket.Context_.getWriteContext(socket.Handle_);
                INTERNAL::setup(context, socket.Context_.PendingIO, IO_OPERATION::IoConnect, 0, nullptr, handler);
                epoll_event ev = {0};
                ev.data.fd = socket.Handle_.value;
                ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
                if (epoll_ctl(socket.Context_.getIOHandle(), EPOLL_CTL_MOD, socket.Handle_.value, &ev) == -1) {
                    return INTERNAL::completeio(context, socket.Context_.PendingIO, TranslateError());
                }
            }
        } else { // connection completed
            epoll_event ev = {0};
            ev.data.fd = socket.Handle_.value;
            ev.events = EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
            if (epoll_ctl(socket.Context_.getIOHandle(), EPOLL_CTL_ADD, socket.Handle_.value, &ev) == -1) {
                 return handler(TranslateError());
            }
            return handler(StatusCode::SC_SUCCESS);
        }
#endif
    }
};

#if _WIN32
INTERNAL::ConnectExCreator AsyncPlatformSocket::ConnectExCreator_;
#endif
typedef SL::NET::AsyncPlatformSocket AsyncSocket;

} // namespace SL::NET
