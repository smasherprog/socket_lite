
#include "Socket_Lite.h"
#include "defs.h"
#include <assert.h>

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
#if _WIN32
    void continue_io(bool success, INTERNAL::Win_IO_RW_Context *context, std::atomic<int> &pendingio)
    {
        if (!success) {
            if (auto handler(context->getCompletionHandler()); handler) {
                context->reset();
                handler(TranslateError(), 0);
            }
        }
        else if (context->bufferlen == context->transfered_bytes) {
            if (auto handler(context->getCompletionHandler()); handler) {
                context->reset();
                handler(StatusCode::SC_SUCCESS, context->transfered_bytes);
            }
        }
        else {
            WSABUF wsabuf;
            auto bytesleft = static_cast<decltype(wsabuf.len)>(context->bufferlen - context->transfered_bytes);
            wsabuf.buf = (char *)context->buffer + context->transfered_bytes;
            wsabuf.len = bytesleft;
            DWORD dwSendNumBytes(0), dwFlags(0);
            pendingio += 1;
            DWORD nRet = 0;
            if (context->IOOperation == INTERNAL::IO_OPERATION::IoRead) {
                nRet = WSARecv(context->Socket_.value, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context->Overlapped), NULL);
            }
            else {
                nRet = WSASend(context->Socket_.value, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context->Overlapped), NULL);
            }
            auto lasterr = WSAGetLastError();
            if (nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                if (auto handler(context->getCompletionHandler()); handler) {
                    pendingio -= 1;
                    context->reset();
                    handler(TranslateError(&lasterr), 0);
                }
            }
        }
    }

    void continue_connect(bool success, Win_IO_Connect_Context *context)
    {
        if (auto h = context->getCompletionHandler(); h) {
            Socket sock(Socket(*context->Context_, std::move(context->Socket_)));
            if (success && ::setsockopt(context->Socket_.Handle().value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
                h(StatusCode::SC_SUCCESS, std::move(sock));
            }
            else {
                h(TranslateError(), std::move(sock));
            }
        }

        if (context->Context_->DecrementPendingIO() == 0) {
            // safe to delete
            delete context;
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
    std::tuple<StatusCode, Socket> connect(Context &c, sockaddr &address, std::function<void(StatusCode, Socket)> &&handler)
    {
        PlatformSocket handle(Family(address));
        auto bindret = BindSocket(handle.Handle().value, Family(address));
        if (bindret != StatusCode::SC_SUCCESS) {
            return std::tuple<StatusCode, Socket>(bindret, Socket(c));
        }
        if (CreateIoCompletionPort((HANDLE)handle.Handle().value, c.ContextImpl_.IOCPHandle, NULL, NULL) == NULL) {
            return std::tuple<StatusCode, Socket>(TranslateError(), Socket(c));
        }

        auto context = new Win_IO_Connect_Context();
        context->Context_ = &c.ContextImpl_;
        context->setCompletionHandler(std::move(handler));
        context->IOOperation = INTERNAL::IO_OPERATION::IoConnect;
        context->Socket_ = std::move(handle);
        c.ContextImpl_.IncrementPendingIO();
        context->IncrementRef();

        auto connectres =
            c.ContextImpl_.ConnectEx_(handle, (::sockaddr *)SocketAddr(address), SocketAddrLen(address), 0, 0, 0, (LPOVERLAPPED)&context->Overlapped);
        if (connectres == TRUE) {
            // connection completed immediatly!
            if (auto h = context->getCompletionHandler(); h) {
                Socket sock(Socket(*context->Context_, std::move(context->Socket_)));
                if (::setsockopt(context->Socket_.Handle().value, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
                    return std::tuple<StatusCode, Socket>(StatusCode::SC_SUCCESS, std::move(sock));
                }
                else {
                    return std::tuple<StatusCode, Socket>(TranslateError(), std::move(sock));
                }
            }

            if (c.ContextImpl_.DecrementPendingIO() == 0) {
                // safe to delete
                delete context;
            }
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            // safe to delete
            delete context;
            return std::tuple<StatusCode, Socket>(TranslateError(), Socket(c));
        }
        return std::tuple<StatusCode, Socket>(StatusCode::SC_PENDINGIO, Socket(c));
    }
#else

    void connect(Socket &socket, SL::NET::sockaddr &address, std::function<void(StatusCode)> &&handler)
    {
        SocketGetter sg(socket);
        auto writecontext = sg.getWriteContext();
        assert(writecontext->IOOperation == INTERNAL::IO_OPERATION::IoNone);
        auto handle = writecontext->Socket_ = INTERNAL::Socket(address.get_Family());
        auto ret = ::connect(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen());
        if (ret == -1) { // will complete some time later
            auto err = errno;
            if (err != EINPROGRESS) { // error with the socket
                handler(TranslateError(&err));
            }
            else {
                // need to allocate for the epoll call
                sg.getPendingIO() += 1;
                writecontext->setCompletionHandler([ihandler(std::move(handler))](StatusCode s, size_t sz) { ihandler(s); });
                writecontext->IOOperation = INTERNAL::IO_OPERATION::IoConnect;

                epoll_event ev = {0};
                ev.data.ptr = writecontext;
                ev.events = EPOLLOUT | EPOLLONESHOT;
                if (epoll_ctl(sg.getIOCPHandle(), EPOLL_CTL_ADD, handle, &ev) == -1) {
                    if (auto h = writecontext->getCompletionHandler(); h) {
                        writecontext->reset();
                        sg.getPendingIO() -= 1;
                        h(TranslateError(), 0);
                    }
                }
            }
        }
        else { // connection completed
            auto[suc, errocode] = INTERNAL::getsockopt_O_ERROR(handle);
            if (suc == StatusCode::SC_SUCCESS && errocode.has_value() && errocode.value() == 0) {
                handler(StatusCode::SC_SUCCESS);
            }
            else {
                handler(TranslateError());
            }
        }
    }
    void continue_connect(bool success, INTERNAL::Win_IO_RW_Context *context)
    {
        auto h = context->getCompletionHandler();
        if (h) {
            context->reset();
            auto[suc, errocode] = INTERNAL::getsockopt_O_ERROR(context->Socket_);
            if (suc == StatusCode::SC_SUCCESS && errocode.has_value() && errocode.value() == 0) {
                h(StatusCode::SC_SUCCESS, 0);
            }
            else {
                h(TranslateError(), 0);
            }
        }
    }
    void continue_io(bool success, INTERNAL::Win_IO_RW_Context *context, std::atomic<int> &pendingio, int iocphandle)
    {
        auto bytestowrite = context->bufferlen - context->transfered_bytes;
        auto count = 0;
        if (context->IOOperation == INTERNAL::IO_OPERATION::IoRead) {
            count = ::read(context->Socket_, context->buffer + context->transfered_bytes, bytestowrite);
            if (count <= 0) { // possible error or continue
                if ((errno != EAGAIN && errno != EINTR) || count == 0) {
                    if (auto h(context->getCompletionHandler()); h) {
                        context->reset();
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
            count = ::write(context->Socket_, context->buffer + context->transfered_bytes, bytestowrite);
            if (count < 0) { // possible error or continue
                if (errno != EAGAIN && errno != EINTR) {
                    if (auto h(context->getCompletionHandler()); h) {
                        context->reset();
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
            auto h(context->getCompletionHandler());
            if (h) {
                context->reset();
                h(StatusCode::SC_SUCCESS, context->bufferlen);
            }
            return; // done with this get out!!
        }

        epoll_event ev = {0};
        ev.data.ptr = context;
        ev.events = context->IOOperation == INTERNAL::IO_OPERATION::IoRead ? EPOLLIN : EPOLLOUT;
        ev.events |= EPOLLONESHOT;
        pendingio += 1;
        if (epoll_ctl(iocphandle, EPOLL_CTL_MOD, context->Socket_, &ev) == -1) {
            auto h(context->getCompletionHandler());
            if (h) {
                context->reset();
                pendingio -= 1;
                h(TranslateError(), 0);
            }
        }
    }

#endif
} // namespace NET
} // namespace SL
