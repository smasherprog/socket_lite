
#include "Context.h"
#include "Socket.h"
#include <Ws2ipdef.h>
#include <assert.h>
namespace SL {
namespace NET {

    Socket::Socket(Context *context, AddressFamily family) : Socket(context) { handle = INTERNAL::Socket(family); }
    Socket::Socket(Context *context) : Context_(context) {}
    Socket::~Socket() { close(); }
    void Socket::close() { ISocket::close(); }
    void Socket::recv(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        assert(ReadContext.IOOperation == IO_OPERATION::IoNone);
        ReadContext.buffer = buffer;
        ReadContext.transfered_bytes = 0;
        ReadContext.bufferlen = buffer_size;
        ReadContext.Context_ = Context_;
        ReadContext.Socket_ = this;
        ReadContext.setCompletionHandler(std::move(handler));
        ReadContext.IOOperation = IO_OPERATION::IoRead;
        continue_io(true, &ReadContext);
    }
    void Socket::send(size_t buffer_size, unsigned char *buffer, std::function<void(StatusCode, size_t)> &&handler)
    {
        assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
        WriteContext.buffer = buffer;
        WriteContext.transfered_bytes = 0;
        WriteContext.bufferlen = buffer_size;
        WriteContext.Context_ = Context_;
        WriteContext.Socket_ = this;
        WriteContext.setCompletionHandler(std::move(handler));
        WriteContext.IOOperation = IO_OPERATION::IoWrite;
        continue_io(true, &WriteContext);
    }
    void Socket::continue_io(bool success, Win_IO_RW_Context *context)
    {
        auto &iocontext = *context->Context_;
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
            iocontext.PendingIO += 1;
            DWORD nRet = 0;
            if (context->IOOperation == IO_OPERATION::IoRead) {
                nRet = WSARecv(context->Socket_->get_handle(), &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(context->Overlapped), NULL);
            }
            else {
                nRet = WSASend(context->Socket_->get_handle(), &wsabuf, 1, &dwSendNumBytes, dwFlags, &(context->Overlapped), NULL);
            }
            auto lasterr = WSAGetLastError();
            if (nRet == SOCKET_ERROR && (WSA_IO_PENDING != lasterr)) {
                if (auto handler(context->getCompletionHandler()); handler) {
                    iocontext.PendingIO -= 1;
                    context->reset();
                    handler(TranslateError(&lasterr), 0);
                }
            }
        }
    }
    void BindSocket(SOCKET sock, AddressFamily family)
    {
        if (family == AddressFamily::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV4);
            INTERNAL::bind(sock, mytestaddr);
        }
        else {
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            bindaddr.sin6_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV6);
            INTERNAL::bind(sock, mytestaddr);
        }
    }

    void Socket::continue_connect(bool success, Win_IO_RW_Context *context)
    {
        auto h = context->getCompletionHandler();
        context->reset();
        if (success && ::setsockopt(context->Socket_->get_handle(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) != SOCKET_ERROR) {
            if (h) {
                h(StatusCode::SC_SUCCESS, 0);
            }
        }
        else if (h) {
            h(TranslateError(), 0);
        }
    }
    void Socket::connect(sockaddr &address, const std::function<void(StatusCode)> &&handler)
    {
        assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
        WriteContext.Context_ = Context_;
        WriteContext.Socket_ = this;
        WriteContext.setCompletionHandler([ihandle(std::move(handler))](StatusCode s, size_t) { ihandle(s); });
        WriteContext.IOOperation = IO_OPERATION::IoConnect;

        ISocket::close();
        handle = INTERNAL::Socket(address.get_Family());
        BindSocket(handle, address.get_Family());
        if (CreateIoCompletionPort((HANDLE)handle, Context_->IOCPHandle, NULL, NULL) == NULL) {
            return handler(TranslateError());
        }
        Context_->PendingIO += 1;
        auto connectres = Context_->ConnectEx_(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen(), 0, 0, 0,
                                               (LPOVERLAPPED)&WriteContext.Overlapped);
        if (connectres == TRUE) {
            Context_->PendingIO -= 1;
            Socket::continue_connect(true, &WriteContext);
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {

            if (auto h(WriteContext.getCompletionHandler()); h) {
                Context_->PendingIO -= 1;
                h(TranslateError(&err), 0);
            }
        }
    }

} // namespace NET
} // namespace SL
