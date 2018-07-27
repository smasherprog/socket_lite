#pragma once
#include "AsyncPlatformSocket.h"

namespace SL::NET {

#if _WIN32
template <class CONTEXTTYPE = Context, class SOCKETHANDLERTYPE>
void complete_accept(StatusCode statuscode, AsyncPlatformSocket<CONTEXTTYPE> &sock, SocketHandle listensocket, const SOCKETHANDLERTYPE &callback)
{
    if (statuscode == StatusCode::SC_SUCCESS) {
        if (::setsockopt(sock.Handle().value, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&listensocket, sizeof(listensocket)) == SOCKET_ERROR ||
            CreateIoCompletionPort((HANDLE)sock.Handle().value, sock.getContext().getIOHandle(), NULL, NULL) == NULL) {
            callback(StatusCode::SC_SUCCESS);
        }
        else {
            callback(TranslateError());
        }
    }
    else {
        callback(statuscode);
    }
}
#endif
template <class CONTEXTTYPE = Context> class AsyncAcceptor {
    AsyncPlatformSocket<CONTEXTTYPE> AsyncPlatformSocket_;

#if _WIN32
    char Buffer[(sizeof(SOCKADDR_STORAGE) + 16) * 2];
    LPFN_ACCEPTEX AcceptEx_;
#endif

  public:
    AsyncAcceptor(AsyncPlatformSocket<CONTEXTTYPE> &&socket) : AsyncPlatformSocket_(std::forward<AsyncPlatformSocket<CONTEXTTYPE>>(socket))
    {
#if _WIN32
        AcceptEx_ = nullptr;
        GUID acceptex_guid = WSAID_ACCEPTEX;
        bytes = 0;
        AcceptEx_ = nullptr;
        WSAIoctl(AsyncPlatformSocket_.Handle().value, SIO_GET_EXTENSION_FUNCTION_POINTER, &acceptex_guid, sizeof(acceptex_guid), &AcceptEx_,
                 sizeof(AcceptEx_), &bytes, NULL, NULL);
        assert(AcceptEx_ != nullptr);
#endif
    }
    ~AsyncAcceptor() {}

    template <class SOCKETHANDLERTYPE> StatusCode accept(const SOCKETHANDLERTYPE &callback)
    {
#if _WIN32

        auto &context = Context_.getWriteContext(Handle.value);
        AsyncPlatformSocket socket(Context_);
        auto newsock = sock.Handle().Value;
        auto &ctx = AsyncPlatformSocket_.getContext();
        setup(context, ctx, IO_OPERATION::IoAccept, 0, nullptr,
              [ sock(std::move(sock)), listensock(Handle.value), cb(std::move(callback)) ](StatusCode sc) {
                  complete_accept(sc, sock, listensock, cb);
              });

        DWORD recvbytes = 0;
        auto nRet = AcceptEx_(Handle.value, newsock, (LPVOID)(Buffer), 0, sizeof(SOCKADDR_STORAGE) + 16, sizeof(SOCKADDR_STORAGE) + 16, &recvbytes,
                              (LPOVERLAPPED) & (context->Overlapped));
        if (nRet == TRUE) {
            completeio(context, ctx, StatusCode::SC_SUCCESS);
        }
        else if (auto err = WSAGetLastError(); !(nRet == FALSE && err == ERROR_IO_PENDING)) {
            completeio(context, ctx, TranslateError(&err));
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

inline void continue_accept(bool success, RW_Context &context, Context &iodata, const SocketHandle &handle)
{
#if _WIN32
    if (success) {
        completeio(context, iodata, StatusCode::SC_SUCCESS);
    }
    else {
        completeio(context, iodata, TranslateError());
    }
#else

#endif
}
} // namespace SL::NET