
#include "Context.h"
#include "Socket.h"
#include <assert.h>

namespace SL {
namespace NET {

    Socket::Socket(Context *context, AddressFamily family) : Socket(context)
    {
        if (family == AddressFamily::IPV4) {
            handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        }
        else {
            handle = WSASocketW(AF_INET6, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        }
    }
    Socket::Socket(Context *context) : Context_(context) {}
    Socket::~Socket() {}

    bool Socket::UpdateIOCP(SOCKET socket, HANDLE *iocp, void *completionkey)
    {
        if (*iocp = CreateIoCompletionPort((HANDLE)socket, *iocp, (DWORD_PTR)completionkey, 0); *iocp != NULL) {
            return true;
        }
        else {
            return false;
        }
    }
    template <typename T> void IOComplete(Socket *s, StatusCode code, size_t bytes, T *context)
    {
        if (code != StatusCode::SC_SUCCESS) {
            s->close();
            bytes = 0;
        }
        auto handler(std::move(context->completionhandler));
        context->clear();
        handler(code, bytes);
    }

    void Socket::recv(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
    {
        Context_->PendingIO += 1;
        assert(ReadContext.IOOperation == IO_OPERATION::IoNone);
        ReadContext.buffer = buffer;
        ReadContext.bufferlen = buffer_size;
        ReadContext.completionhandler = std::move(handler);
        ReadContext.IOOperation = IO_OPERATION::IoRead;
        DWORD dwSendNumBytes(static_cast<DWORD>(ReadContext.bufferlen - ReadContext.transfered_bytes)), dwFlags(0);
        // still more data to read.. keep going!
        WSABUF wsabuf;
        wsabuf.buf = (char *)ReadContext.buffer + ReadContext.transfered_bytes;
        wsabuf.len = dwSendNumBytes;

        // do read with zero bytes to prevent memory from being mapped
        if (auto nRet = WSARecv(handle, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(ReadContext.Overlapped), NULL);
            nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            Context_->PendingIO -= 1;
            return IOComplete(this, TranslateError(&nRet), 0, &ReadContext);
        }
    }

    void Socket::continue_read(bool success, Win_IO_RW_Context *sockcontext)
    {
        if (!success) {
            return IOComplete(this, TranslateError(), 0, sockcontext);
        }
        else if (sockcontext->bufferlen == sockcontext->transfered_bytes) {
            return IOComplete(this, StatusCode::SC_SUCCESS, sockcontext->transfered_bytes, sockcontext);
        }
        else {
            Context_->PendingIO += 1;
            DWORD dwSendNumBytes(static_cast<DWORD>(sockcontext->bufferlen - sockcontext->transfered_bytes)), dwFlags(0);
            // still more data to read.. keep going!
            WSABUF wsabuf;
            wsabuf.buf = (char *)sockcontext->buffer + sockcontext->transfered_bytes;
            wsabuf.len = dwSendNumBytes;
            // do read with zero bytes to prevent memory from being mapped
            if (auto nRet = WSARecv(handle, &wsabuf, 1, &dwSendNumBytes, &dwFlags, &(sockcontext->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                Context_->PendingIO -= 1;
                return IOComplete(this, TranslateError(&nRet), 0, sockcontext);
            }
        }
    }

    void Socket::connect(sockaddr &address, const std::function<void(StatusCode)> &&handler)
    {
        ISocket::close();
        ReadContext.clear();
        if (address.get_Family() == AddressFamily::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV4);
            if (auto b = bind(mytestaddr); b != StatusCode::SC_SUCCESS) {
                return handler(b);
            }
        }
        else {
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            bindaddr.sin6_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV6);
            if (auto b = bind(mytestaddr); b != StatusCode::SC_SUCCESS) {
                return handler(b);
            }
        }

        if (!Context_->ConnectEx_) {
            GUID guid = WSAID_CONNECTEX;
            DWORD bytes = 0;
            if (WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &Context_->ConnectEx_, sizeof(Context_->ConnectEx_), &bytes,
                         NULL, NULL) == SOCKET_ERROR) {
                return handler(TranslateError());
            }
        }
        if (!Socket::UpdateIOCP(handle, &Context_->iocp.handle, this)) {
            return handler(TranslateError());
        }

        ReadContext.completionhandler = [ihandle(std::move(handler))](StatusCode code, size_t) { ihandle(code); };
        ;
        ReadContext.IOOperation = IO_OPERATION::IoConnect;
        Context_->PendingIO += 1;
        DWORD bytessend = 0;
        auto connectres = Context_->ConnectEx_(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen(), NULL, 0, &bytessend,
                                               (LPOVERLAPPED)&ReadContext.Overlapped);
        if (connectres == TRUE) {
            Context_->PendingIO -= 1;
            auto chandle(std::move(ReadContext.completionhandler));
            chandle(StatusCode::SC_SUCCESS, 0);
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            Context_->PendingIO -= 1;
            auto chandle(std::move(ReadContext.completionhandler));
            chandle(TranslateError(&err), 0);
        }
    }
    void Socket::continue_connect(StatusCode connect_success, Win_IO_RW_Context *sockcontext)
    {
        auto chandle(std::move(sockcontext->completionhandler));
        sockcontext->clear();
        if (connect_success == StatusCode::SC_SUCCESS && ::setsockopt(handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) == SOCKET_ERROR) {
            chandle(TranslateError(), 0);
        }
        else {
            chandle(StatusCode::SC_SUCCESS, 0);
        }
    }
    void Socket::send(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
    {
        assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
        WriteContext.buffer = buffer;
        WriteContext.bufferlen = buffer_size;
        WriteContext.completionhandler = std::move(handler);
        WriteContext.IOOperation = IO_OPERATION::IoWrite;
        continue_write(true, &WriteContext);
    }
    void Socket::continue_write(bool success, Win_IO_RW_Context *sockcontext)
    {
        if (!success) {
            return IOComplete(this, TranslateError(), 0, sockcontext);
        }
        else if (sockcontext->transfered_bytes == sockcontext->bufferlen) {
            return IOComplete(this, StatusCode::SC_SUCCESS, sockcontext->transfered_bytes, sockcontext);
        }
        else {
            Context_->PendingIO += 1;
            WSABUF wsabuf;
            wsabuf.buf = (char *)sockcontext->buffer + sockcontext->transfered_bytes;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(sockcontext->bufferlen - sockcontext->transfered_bytes);
            DWORD dwSendNumBytes(0), dwFlags(0);
            if (auto nRet = WSASend(handle, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(sockcontext->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                Context_->PendingIO -= 1;
                return IOComplete(this, TranslateError(&nRet), 0, sockcontext);
            }
        }
    }

} // namespace NET
} // namespace SL
