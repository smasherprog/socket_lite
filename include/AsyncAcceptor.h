#pragma once
#include "AsyncPlatformSocket.h"

namespace SL::NET {

template <class SOCKETHANDLERTYPE>
void complete_accept(StatusCode statuscode, SocketHandle sock, SocketHandle listensocket, const SOCKETHANDLERTYPE &callback, Context &ctx)
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
class AsyncPlatformAcceptor {
    AsyncPlatformSocket AcceptorSocket;
    AddressFamily AddressFamily_ = AddressFamily::IPANY;
    Context &Context_;
#if _WIN32
    char Buffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
    LPFN_ACCEPTEX AcceptEx_;

#endif

  public:
    AsyncPlatformAcceptor(AsyncPlatformSocket &&socket) : Context_(socket.getContext()), AcceptorSocket(std::forward<AsyncPlatformSocket>(socket))
    {
        auto status = AcceptorSocket.getsockname([&](const SocketAddress &addr) { AddressFamily_ = addr.getFamily(); });
        if (status != StatusCode::SC_SUCCESS) {
            abort();
        }
#if _WIN32
        AcceptEx_ = nullptr;
        GUID acceptex_guid = WSAID_ACCEPTEX;
        DWORD bytes = 0;
        AcceptEx_ = nullptr;
        WSAIoctl(AcceptorSocket.Handle().value, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_,
                 sizeof(AcceptEx_), &bytes, NULL, NULL);
        assert(AcceptEx_ != nullptr);
#endif
    }
    ~AsyncPlatformAcceptor() {}

    template <class SOCKETHANDLERTYPE> void accept(const SOCKETHANDLERTYPE &callback)
    {
#if _WIN32
        SocketHandle sockethandle(INVALID_SOCKET);
        if (AddressFamily_ == AddressFamily::IPV4) {
            sockethandle.value = ::socket(AF_INET, SOCK_STREAM, 0);
        }
        else {
            sockethandle.value = ::socket(AF_INET6, SOCK_STREAM, 0);
        }
        u_long iMode = 1;
        ioctlsocket(sockethandle.value, FIONBIO, &iMode);

        auto listensock(AcceptorSocket.Handle());
        auto &context = Context_.getWriteContext(listensock);
        setup(context, Context_, IO_OPERATION::IoAccept, 0, nullptr,
              [ sockethandle, listensock, cb(std::move(callback)), &ctx = Context_ ](StatusCode sc) {
                  complete_accept(sc, sockethandle, listensock, cb, ctx);
              });

        DWORD recvbytes = 0;
        auto nRet = AcceptEx_(listensock.value, sockethandle.value, (LPVOID)(Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16,
                              &recvbytes, (LPOVERLAPPED) & (context.Overlapped));
        if (nRet == TRUE) {
            completeio(context, Context_, StatusCode::SC_SUCCESS);
        }
        else if (auto err = WSAGetLastError(); !(nRet == FALSE && err == ERROR_IO_PENDING)) {
            completeio(context, Context_, TranslateError(&err));
        }

#else

        auto handle = ::accept4(ListenSocket.Handle().value, NULL, NULL, SOCK_NONBLOCK);
        if (handle != INVALID_SOCKET) {
            AsyncPlatformSocket s(handle);
            epoll_event ev = {0};
            ev.events = EPOLLONESHOT;
            if (epoll_ctl(Context_.getIOHandle(), EPOLL_CTL_ADD, handle, &ev) == 0) {
                AcceptHandler(Socket_impl(Context_, std::move(s)));
            }
        }
#endif
    }
}; 
typedef SL::NET::AsyncPlatformAcceptor AsyncAcceptor;
} // namespace SL::NET