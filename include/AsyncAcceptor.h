#pragma once
#include "AsyncPlatformSocket.h"
#include "Internal.h"

namespace SL::NET {

class AsyncPlatformAcceptor {
#if _WIN32
    template <class SOCKETHANDLERTYPE>
    static void complete_accept(StatusCode statuscode, SocketHandle sock, SocketHandle listensocket, const SOCKETHANDLERTYPE &callback,
                                Context &ctx) noexcept
    {
        if (statuscode == StatusCode::SC_SUCCESS) {
            auto lsock = listensocket.value;
            if (::setsockopt(sock.value, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&lsock, sizeof(lsock)) == SOCKET_ERROR ||
                CreateIoCompletionPort((HANDLE)sock.value, ctx.getIOHandle(), sock.value, NULL) == NULL) {
                callback(TranslateError(), AsyncPlatformSocket(ctx, sock));
            }
            else {
                callback(StatusCode::SC_SUCCESS, AsyncPlatformSocket(ctx, sock));
            }
        }
        else {
            callback(statuscode, AsyncPlatformSocket(ctx, sock));
        }
    }
    static INTERNAL::AcceptExCreator AcceptExCreator_;
    char Buffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2] = {};
#endif
    AsyncPlatformSocket AcceptorSocket;
    AddressFamily AddressFamily_ = AddressFamily::IPANY;
    Context &Context_;

  public:
    AsyncPlatformAcceptor(AsyncPlatformSocket &&socket) noexcept
        : Context_(socket.getContext()), AcceptorSocket(std::forward<AsyncPlatformSocket>(socket))
    {
        auto status = AcceptorSocket.getsockname([&](const SocketAddress &addr) { AddressFamily_ = addr.getFamily(); });
        if (status != StatusCode::SC_SUCCESS) {
            abort();
        }
    }
    ~AsyncPlatformAcceptor() noexcept {}

    template <class SOCKETHANDLERTYPE> void accept(const SOCKETHANDLERTYPE &callback) noexcept
    {
#if _WIN32
        SocketHandle sockethandle(::socket(AddressFamily_, SOCK_STREAM, 0));
        u_long iMode = 1;
        ioctlsocket(sockethandle.value, FIONBIO, &iMode);

        auto listensock(AcceptorSocket.Handle());
        auto &context = Context_.getWriteContext(listensock);
        INTERNAL::setup(context, Context_.PendingIO, IO_OPERATION::IoAccept, 0, nullptr,
              [sockethandle, listensock, cb(std::move(callback)), &ctx = Context_](StatusCode sc) {
                  complete_accept(sc, sockethandle, listensock, cb, ctx);
              });

        DWORD recvbytes = 0;
        auto nRet = AcceptExCreator_.AcceptEx_(listensock.value, sockethandle.value, (LPVOID)(Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16,
                                               sizeof(SOCKADDR_STORAGE) + 16, &recvbytes, (LPOVERLAPPED) & (context.Overlapped));
        if (nRet == TRUE) {
            INTERNAL::completeio(context, Context_.PendingIO, StatusCode::SC_SUCCESS);
        }
        else if (auto err = WSAGetLastError(); !(nRet == FALSE && err == ERROR_IO_PENDING)) {
            INTERNAL::completeio(context, Context_.PendingIO, TranslateError(&err));
        }

#else

        auto handle = ::accept4(ListenSocket.Handle().value, NULL, NULL, SOCK_NONBLOCK);
        if (handle != INVALID_SOCKET) {
            AsyncPlatformSocket s(handle);
            epoll_event ev = {0};
            ev.events = EPOLLONESHOT;
            if (epoll_ctl(Context_.getIOHandle(), EPOLL_CTL_ADD, handle, &ev) == 0) {
                completeio(context, Context_, TranslateError(&err));
            }
        }
#endif
    }
};
#if _WIN32
INTERNAL::AcceptExCreator AsyncPlatformAcceptor::AcceptExCreator_;
#endif
typedef SL::NET::AsyncPlatformAcceptor AsyncAcceptor;
} // namespace SL::NET