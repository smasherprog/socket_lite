
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

namespace SL
{
namespace NET
{
void completeio(Win_IO_Context &context, ContextImpl &iodata, StatusCode code, size_t bytes)
{
    if (auto h(context.getCompletionHandler()); h) {
        iodata.DecrementPendingIO();
        h(code, bytes);
    }
}
Socket::Socket(ContextImpl &c) : IOData_(c) {}
Socket::Socket(ContextImpl &c, PlatformSocket &&p) : Socket(c)
{
    PlatformSocket_ = std::move(p);
    IOData_.RegisterSocket(PlatformSocket_.Handle());
}
Socket::Socket(Context &c, PlatformSocket &&p) : Socket(*c.ContextImpl_, std::move(p)) {}
Socket::Socket(Context &c) : Socket(*c.ContextImpl_) {}
Socket::Socket(Socket &&sock) : Socket(sock.IOData_, std::move(sock.PlatformSocket_)) {}
Socket::~Socket()
{
    
    IOData_.DeregisterSocket(PlatformSocket_.Handle());
    PlatformSocket_.close();
}
void Socket::close()
{
    PlatformSocket_.shutdown(ShutDownOptions::SHUTDOWN_BOTH);
}
void Socket::recv_async(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
{
    auto handle = PlatformSocket_.Handle();
    if (handle.value == INVALID_SOCKET) {
        return handler(StatusCode::SC_CLOSED, 0); // socket is closed..
    }

    auto &ReadContext_ = IOData_.getReadContext(handle);
    ReadContext_.buffer = buffer;
    ReadContext_.transfered_bytes = 0;
    ReadContext_.bufferlen = buffer_size;
    ReadContext_.setCompletionHandler(std::move(handler));
    ReadContext_.IOOperation = IO_OPERATION::IoRead;
    IOData_.IncrementPendingIO();
//std::cout<<"read"<<handle.value<<std::endl;
#if _WIN32
    ReadContext_.Overlapped = {0};

    auto count = ::recv(handle.value, (char *)buffer, buffer_size, 0);
    if (count <= 0) { // possible error or continue
        if (auto er = WSAGetLastError(); er != WSAEWOULDBLOCK) {
            completeio(ReadContext_, IOData_, TranslateError(), 0);
        } else {
            continue_io(true, ReadContext_, IOData_, PlatformSocket_.Handle());
        }
    }
    else { 
        static int long long depth = 0;
        if (depth++%5==0){
            PostQueuedCompletionStatus(IOData_.getIOHandle(), count, handle.value, &(ReadContext_.Overlapped));
        }
        else {
            ReadContext_.transfered_bytes = count;
            continue_io(true, ReadContext_, IOData_, PlatformSocket_.Handle());
        }
    }
#else

    auto count = ::read(handle.value, buffer, buffer_size);
    if (count <= 0) { // possible error or continue
        if ((errno != EAGAIN && errno != EINTR) || count == 0) {
            return completeio(ReadContext_, IOData_, TranslateError(), 0);
        }
        epoll_event ev = {0};
        ev.data.fd = handle.value;
        ev.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        if (epoll_ctl(IOData_.getIOHandle(), EPOLL_CTL_MOD, handle.value, &ev) == -1) {
            return completeio(ReadContext_, IOData_, TranslateError(), 0);
        }
    } else {

        ReadContext_.transfered_bytes = count;
        IOData_.wakeupReadfd(handle.value);
    }


#endif

}
void Socket::send_async(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
{
    auto handle = PlatformSocket_.Handle();
    if (handle.value == INVALID_SOCKET) {
        return handler(StatusCode::SC_CLOSED, 0); // socket is closed..
    }

    auto &WriteContext_ = IOData_.getWriteContext(handle);
    WriteContext_.buffer = buffer;
    WriteContext_.transfered_bytes = 0;
    WriteContext_.bufferlen = buffer_size;
    WriteContext_.setCompletionHandler(std::move(handler));
    WriteContext_.IOOperation = IO_OPERATION::IoWrite;

//std::cout<<"write"<<handle.value<<std::endl;
    IOData_.IncrementPendingIO();
#if _WIN32
    WriteContext_.Overlapped = {0};

    auto count = ::send(handle.value, (char *)buffer, buffer_size, 0);
    if (count < 0) { // possible error or continue
        if (auto er = WSAGetLastError(); er != WSAEWOULDBLOCK) {
            completeio(WriteContext_, IOData_, TranslateError(), 0);
        }
        else {
            continue_io(true, WriteContext_, IOData_, PlatformSocket_.Handle());
        }
    }
    else {
        PostQueuedCompletionStatus(IOData_.getIOHandle(), count, handle.value, &(WriteContext_.Overlapped));
    }
#else
    auto count = ::write(handle.value,buffer, buffer_size);
    if (count < 0) { // possible error or continue
        if (errno != EAGAIN && errno != EINTR) {
            return completeio(WriteContext_, IOData_, TranslateError(), 0);
        }

        epoll_event ev = {0};
        ev.data.fd = handle.value;
        ev.events = EPOLLOUT| EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        if (epoll_ctl(IOData_.getIOHandle(), EPOLL_CTL_MOD, handle.value, &ev) == -1) {
            return completeio(WriteContext_, IOData_, TranslateError(), 0);
        }
    } else {

        WriteContext_.transfered_bytes = count;
        IOData_.wakeupWritefd(handle.value);
    }

#endif

}
#if _WIN32
void continue_io(bool success, Win_IO_Context &context, ContextImpl &iodata, const SocketHandle &handle)
{
    if (!success) {
        completeio(context, iodata, TranslateError(), 0);
    } else if (context.bufferlen == context.transfered_bytes) {
        completeio(context, iodata, StatusCode::SC_SUCCESS, context.transfered_bytes);
    } else {
        WSABUF wsabuf = {0};
        auto bytesleft = static_cast<decltype(wsabuf.len)>(context.bufferlen - context.transfered_bytes);
        wsabuf.buf = (char *)context.buffer + context.transfered_bytes;
        wsabuf.len = bytesleft;
        DWORD dwSendNumBytes(0), dwFlags(0);
        DWORD nRet = 0;

        if (context.IOOperation == IO_OPERATION::IoRead) {
            nRet = WSARecv(handle.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context.Overlapped), NULL);
        } else {
            nRet = WSASend(handle.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context.Overlapped), NULL);
        }
        auto lasterr = WSAGetLastError();
        if (nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
            completeio(context, iodata, TranslateError(&lasterr), 0);
        }
    }
}
void continue_connect(bool success, Win_IO_Context &context, ContextImpl &iodata, const SocketHandle &handle)
{
    if (success && ::setsockopt(handle.value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
        completeio(context, iodata, StatusCode::SC_SUCCESS, 0);
    } else {
        completeio(context, iodata, TranslateError(), 0);
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
    } else {
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
void connect_async(Socket &socket, SocketAddress &address, std::function<void(StatusCode)> &&handler)
{
    socket.PlatformSocket_ = PlatformSocket(Family(address), Blocking_Options::NON_BLOCKING);
    auto handle = socket.PlatformSocket_.Handle();
    if (handle.value == INVALID_SOCKET) {
        return handler(StatusCode::SC_CLOSED); // socket is closed..
    }
    auto bindret = BindSocket(handle.value, Family(address));
    if (bindret != StatusCode::SC_SUCCESS) {
        return handler(bindret);
    }
    if (CreateIoCompletionPort((HANDLE)handle.value, socket.IOData_.getIOHandle(), handle.value, NULL) == NULL) {
        return handler(TranslateError());
    }

    socket.IOData_.RegisterSocket(handle);
    auto &context = socket.IOData_.getWriteContext(handle);
    context.setCompletionHandler([ihandler(std::move(handler))](StatusCode s, size_t) {
        ihandler(s);
    });
    context.IOOperation = IO_OPERATION::IoConnect;
    socket.IOData_.IncrementPendingIO();

    auto connectres = socket.IOData_.ConnectEx_(handle.value, (::sockaddr *)SocketAddr(address), SocketAddrLen(address), 0, 0, 0,
                      (LPOVERLAPPED)&context.Overlapped);
    if (connectres == TRUE) {
        // connection completed immediatly!
        if (::setsockopt(handle.value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
            completeio(context, socket.IOData_, StatusCode::SC_SUCCESS, 0);
        } else {
            completeio(context, socket.IOData_, TranslateError(), 0);
        }
    } else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
        completeio(context, socket.IOData_, TranslateError(), 0);
    }
}
#else

void connect_async(Socket &socket, SocketAddress &address, std::function<void(StatusCode)> &&handler)
{
    socket.PlatformSocket_ = PlatformSocket(Family(address), Blocking_Options::NON_BLOCKING);
    auto handle = socket.PlatformSocket_.Handle();
    if (handle.value == INVALID_SOCKET) {
        return handler(StatusCode::SC_CLOSED); // socket is closed..
    }
    socket.IOData_.RegisterSocket(handle);
    auto ret = ::connect(handle.value, (::sockaddr *)SocketAddr(address), SocketAddrLen(address));
    if (ret == -1) { // will complete some time later
        auto err = errno;
        if (err != EINPROGRESS) { // error with the socket
            return handler(TranslateError(&err));
        } else {
            auto &context = socket.IOData_.getWriteContext(handle);
            context.setCompletionHandler([ihandler(std::move(handler))](StatusCode s, size_t) {
                ihandler(s);
            });
            context.IOOperation = IO_OPERATION::IoConnect;
            socket.IOData_.IncrementPendingIO();

            epoll_event ev = {0};
            ev.data.fd = handle.value;
            ev.events = EPOLLOUT | EPOLLONESHOT| EPOLLRDHUP | EPOLLHUP;
            if (epoll_ctl(socket.IOData_.getIOHandle(), EPOLL_CTL_ADD, handle.value, &ev) == -1) {
                return completeio(context, socket.IOData_, TranslateError(), 0);
            }
        }
    } else { // connection completed
        handler(StatusCode::SC_SUCCESS);
    }
}

void continue_connect(bool success, Win_IO_Context &context, ContextImpl &iodata, const SocketHandle &)
{
    if (success) {
        completeio(context, iodata, StatusCode::SC_SUCCESS, 0);
    } else {
        completeio(context, iodata, StatusCode::SC_CLOSED, 0);
    }
}
void continue_io(bool success, Win_IO_Context &context, ContextImpl &iodata, const SocketHandle &handle)
{

    if (!success) {
        return completeio(context, iodata, StatusCode::SC_CLOSED, 0);
    } else if (context.bufferlen == context.transfered_bytes) {
        return completeio(context, iodata, StatusCode::SC_SUCCESS, context.transfered_bytes);
    } else {
        auto bytestowrite = context.bufferlen - context.transfered_bytes;
        auto count = 0;
        if (context.IOOperation == IO_OPERATION::IoRead) {
            count = ::read(handle.value, context.buffer + context.transfered_bytes, bytestowrite);
            if (count <= 0) { // possible error or continue
                if ((errno != EAGAIN && errno != EINTR) || count == 0) {
                    return completeio(context, iodata, TranslateError(), 0);
                } else {
                    count = 0;
                }
            }
        } else {
            count = ::write(handle.value, context.buffer + context.transfered_bytes, bytestowrite);
            if (count < 0) { // possible error or continue
                if (errno != EAGAIN && errno != EINTR) {
                    return completeio(context, iodata, TranslateError(), 0);
                } else {
                    count = 0;
                }
            }
        }
        context.transfered_bytes += count;
        if (context.transfered_bytes == context.bufferlen) {
            return completeio(context, iodata, StatusCode::SC_SUCCESS, context.bufferlen);
        }

        epoll_event ev = {0};
        ev.data.fd = handle.value;
        ev.events = context.IOOperation == IO_OPERATION::IoRead ? EPOLLIN : EPOLLOUT;
        ev.events |= EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP;
        if (epoll_ctl(iodata.getIOHandle(), EPOLL_CTL_MOD, handle.value, &ev) == -1) {
            return completeio(context, iodata, TranslateError(), 0);
        }
    }
}

#endif
} // namespace NET
} // namespace SL
