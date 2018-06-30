#include "Internal/Context.h"
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
    void completeio(RW_Context &context, Context &iodata, StatusCode code)
    {
        if (auto h(context.getCompletionHandler()); h) {
            iodata.DecrementPendingIO();
            h(code);
        }
    }

    template <class FUNC> void setup(RW_Context &context, Context &iodata, IO_OPERATION op, int buffer_size, unsigned char *buffer, FUNC &&handler)
    {
        context.buffer = buffer;
        context.remaining_bytes = buffer_size;
        context.setCompletionHandler(std::move(handler));
        context.IOOperation = op;
#if _WIN32
        context.Overlapped = {0};
#endif
        iodata.IncrementPendingIO();
    }

    Socket::Socket(Context &c) : IOData_(c) {}
    Socket::Socket(Context &c, PlatformSocket &&p) : Socket(c)
    {
        PlatformSocket_ = std::move(p);
        IOData_.RegisterSocket(PlatformSocket_.Handle());
    }
    Socket::Socket(Socket &&sock) : Socket(sock.IOData_, std::move(sock.PlatformSocket_)) {}
    Socket::~Socket()
    {
        IOData_.DeregisterSocket(PlatformSocket_.Handle());
        PlatformSocket_.close();
    }
    void Socket::close() { PlatformSocket_.shutdown(ShutDownOptions::SHUTDOWN_BOTH); }

    void Socket::recv_success(int buffer_size, unsigned char *buffer, std::function<void(StatusCode)> &&handler, int bytestransfered)
    {
        auto &ReadContext_ = IOData_.getReadContext(PlatformSocket_.Handle());
        setup(ReadContext_, IOData_, IO_OPERATION::IoRead, buffer_size, buffer, std::move(handler));
#if _WIN32
        PostQueuedCompletionStatus(IOData_.getIOHandle(), bytestransfered, PlatformSocket_.Handle().value, &(ReadContext_.Overlapped));
#else
        ReadContext_.remaining_bytes -= bytestransfered;
        ReadContext_.buffer += bytestransfered;
        IOData_.wakeupReadfd(PlatformSocket_.Handle().value);
#endif
    }
    void Socket::recv_continue(int buffer_size, unsigned char *buffer, std::function<void(StatusCode)> &&handler)
    {
        auto &ReadContext_ = IOData_.getReadContext(PlatformSocket_.Handle());
        setup(ReadContext_, IOData_, IO_OPERATION::IoRead, buffer_size, buffer, std::move(handler));
        continue_io(true, ReadContext_, IOData_, PlatformSocket_.Handle());
    }
    void Socket::send_success(int buffer_size, unsigned char *buffer, std::function<void(StatusCode)> &&handler, int bytestransfered)
    {
        auto &WriteContext_ = IOData_.getWriteContext(PlatformSocket_.Handle());
        setup(WriteContext_, IOData_, IO_OPERATION::IoWrite, buffer_size, buffer, std::move(handler));
#if _WIN32
        PostQueuedCompletionStatus(IOData_.getIOHandle(), bytestransfered, PlatformSocket_.Handle().value, &(WriteContext_.Overlapped));
#else
        WriteContext_.remaining_bytes -= bytestransfered;
        WriteContext_.buffer += bytestransfered;
        IOData_.wakeupWritefd(PlatformSocket_.Handle().value);
#endif
    }
    void Socket::send_continue(int buffer_size, unsigned char *buffer, std::function<void(StatusCode)> &&handler)
    {
        auto &WriteContext_ = IOData_.getWriteContext(PlatformSocket_.Handle());
        setup(WriteContext_, IOData_, IO_OPERATION::IoWrite, buffer_size, buffer, std::move(handler));
        continue_io(true, WriteContext_, IOData_, PlatformSocket_.Handle());
    }

#if _WIN32
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
    void continue_io(bool success, RW_Context &context, Context &iodata, const SocketHandle &handle)
    {
        if (!success) {
            completeio(context, iodata, TranslateError());
        }
        else if (context.remaining_bytes == 0) {
            completeio(context, iodata, StatusCode::SC_SUCCESS);
        }
        else {
            WSABUF wsabuf;
            wsabuf.buf = (char *)context.buffer;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(context.remaining_bytes);
            DWORD dwSendNumBytes(0), dwFlags(0);
            DWORD nRet = 0;

            if (context.IOOperation == IO_OPERATION::IoRead) {
                nRet = WSARecv(handle.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context.Overlapped), NULL);
            }
            else {
                nRet = WSASend(handle.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context.Overlapped), NULL);
            }
            auto lasterr = WSAGetLastError();
            if (nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                completeio(context, iodata, TranslateError(&lasterr));
            }
        }
    }
    void continue_connect(bool success, RW_Context &context, Context &iodata, const SocketHandle &handle)
    {
        if (success && ::setsockopt(handle.value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) == 0) {
            completeio(context, iodata, StatusCode::SC_SUCCESS);
        }
        else {
            completeio(context, iodata, TranslateError());
        }
    }

    void connect_async(Socket &socket, SocketAddress &address, std::function<void(StatusCode)> &&handler)
    {
        auto handle = PlatformSocket(Family(address), Blocking_Options::NON_BLOCKING);
        if (handle.Handle().value == INVALID_SOCKET) {
            return handler(StatusCode::SC_CLOSED); // socket is closed..
        }
        auto bindret = BindSocket(handle.Handle().value, Family(address));
        if (bindret != StatusCode::SC_SUCCESS) {
            return handler(bindret);
        }
        if (CreateIoCompletionPort((HANDLE)handle.Handle().value, socket.IOData_.getIOHandle(), handle.Handle().value, NULL) == NULL) {
            return handler(TranslateError());
        }
        socket.PlatformSocket_ = std::move(handle);
        auto hhandle = socket.PlatformSocket_.Handle().value;
        socket.IOData_.RegisterSocket(socket.PlatformSocket_.Handle());
        auto &context = socket.IOData_.getWriteContext(socket.PlatformSocket_.Handle());

        context.setCompletionHandler(std::move(handler));
        context.IOOperation = IO_OPERATION::IoConnect;
        socket.IOData_.IncrementPendingIO();

        auto connectres =
            socket.IOData_.ConnectEx_(hhandle, (::sockaddr *)SocketAddr(address), SocketAddrLen(address), 0, 0, 0, (LPOVERLAPPED)&context.Overlapped);
        if (connectres == TRUE) {
            // connection completed immediatly!
            if (::setsockopt(hhandle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
                completeio(context, socket.IOData_, StatusCode::SC_SUCCESS);
            }
            else {
                completeio(context, socket.IOData_, TranslateError());
            }
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            completeio(context, socket.IOData_, TranslateError());
        }
    }
#else

    void connect_async(Socket &socket, SocketAddress &address, std::function<void(StatusCode)> &&handler)
    {
        auto handle = PlatformSocket(Family(address), Blocking_Options::NON_BLOCKING);
        if (handle.Handle().value == INVALID_SOCKET) {
            return handler(StatusCode::SC_CLOSED); // socket is closed..
        }
        auto hhandle = handle.Handle().value;
        socket.PlatformSocket_ = std::move(handle);
        socket.IOData_.RegisterSocket(socket.PlatformSocket_.Handle());
        auto ret = ::connect(hhandle, (::sockaddr *)SocketAddr(address), SocketAddrLen(address));
        if (ret == -1) { // will complete some time later
            auto err = errno;
            if (err != EINPROGRESS) { // error with the socket
                return handler(TranslateError(&err));
            }
            else {
                auto &context = socket.IOData_.getWriteContext(socket.PlatformSocket_.Handle());
                context.setCompletionHandler(std::move(handler));
                context.IOOperation = IO_OPERATION::IoConnect;
                socket.IOData_.IncrementPendingIO();

                epoll_event ev = {0};
                ev.data.fd = hhandle;
                ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
                if (epoll_ctl(socket.IOData_.getIOHandle(), EPOLL_CTL_ADD, hhandle, &ev) == -1) {
                    return completeio(context, socket.IOData_, TranslateError());
                }
            }
        }
        else { // connection completed
            handler(StatusCode::SC_SUCCESS);
        }
    }

    void continue_connect(bool success, RW_Context &context, Context &iodata, const SocketHandle &)
    {
        if (success) {
            completeio(context, iodata, StatusCode::SC_SUCCESS);
        }
        else {
            completeio(context, iodata, StatusCode::SC_CLOSED);
        }
    }
    void continue_io(bool success, RW_Context &context, Context &iodata, const SocketHandle &handle)
    {

        if (!success) {
            return completeio(context, iodata, StatusCode::SC_CLOSED);
        }
        else if (context.remaining_bytes == 0) {
            return completeio(context, iodata, StatusCode::SC_SUCCESS);
        }
        else {
            auto count = 0;
            if (context.IOOperation == IO_OPERATION::IoRead) {
                count = ::recv(handle.value, context.buffer, context.remaining_bytes, MSG_NOSIGNAL);
                if (count <= 0) { // possible error or continue
                    if ((errno != EAGAIN && errno != EINTR) || count == 0) {
                        return completeio(context, iodata, TranslateError());
                    }
                    else {
                        count = 0;
                    }
                }
            }
            else {
                count = ::send(handle.value, context.buffer, context.remaining_bytes, MSG_NOSIGNAL);
                if (count < 0) { // possible error or continue
                    if (errno != EAGAIN && errno != EINTR) {
                        return completeio(context, iodata, TranslateError());
                    }
                    else {
                        count = 0;
                    }
                }
            }
            context.buffer += count;
            context.remaining_bytes -= count;
            if (context.remaining_bytes == 00) {
                return completeio(context, iodata, StatusCode::SC_SUCCESS);
            }

            epoll_event ev = {0};
            ev.data.fd = handle.value;
            ev.events = context.IOOperation == IO_OPERATION::IoRead ? EPOLLIN : EPOLLOUT;
            ev.events |= EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
            if (epoll_ctl(iodata.getIOHandle(), EPOLL_CTL_MOD, handle.value, &ev) == -1) {
                return completeio(context, iodata, TranslateError());
            }
        }
    }

#endif
} // namespace NET
} // namespace SL
