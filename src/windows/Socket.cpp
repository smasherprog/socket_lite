
#include "Context.h"
#include "Socket.h"
#include <assert.h>

namespace SL {
namespace NET {

    LPFN_CONNECTEX ConnectEx_ = nullptr;
    Socket::Socket(Context *context, AddressFamily family) : Socket(context) { handle = context->getSocket(family); }
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

    void Socket::continue_read(bool success)
    {
        if (!success) {
            return IOComplete(this, TranslateError(), 0, &ReadContext);
        }
        else if (ReadContext.bufferlen == ReadContext.transfered_bytes) {
            return IOComplete(this, StatusCode::SC_SUCCESS, ReadContext.transfered_bytes, &ReadContext);
        }
        else {
            Context_->PendingIO += 1;
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
    }

    void Socket::init_connect(bool success, Win_IO_Connect_Context *context)
    {
        static auto iodone = [](auto code, auto context) {
            auto chandle(std::move(context->completionhandler));
            delete context;
            chandle(code);
        };
        if (!success) {
            return iodone(TranslateError(), context);
        }
        ISocket::close();
        handle = Context_->getSocket(context->address.get_Family());
        if (context->address.get_Family() == AddressFamily::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV4);
            INTERNAL::bind(handle, mytestaddr);
        }
        else {
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            bindaddr.sin6_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, AddressFamily::IPV6);
            INTERNAL::bind(handle, mytestaddr);
        }
        if (!INTERNAL::setsockopt_O_BLOCKING(handle, Blocking_Options::NON_BLOCKING)) {
            return iodone(TranslateError(), context);
        }

        if (!Socket::UpdateIOCP(handle, &Context_->iocp.handle, this)) {
            return iodone(TranslateError(), context);
        }

        context->IOOperation = IO_OPERATION::IoConnect;
        context->Overlapped = {0};
        Context_->PendingIO += 1;
        DWORD bytessend = 0;
        auto connectres = ConnectEx_(handle, (::sockaddr *)context->address.get_SocketAddr(), context->address.get_SocketAddrLen(), NULL, 0,
                                     &bytessend, (LPOVERLAPPED)&context->Overlapped);
        if (connectres == TRUE) {
            Context_->PendingIO -= 1;
            iodone(StatusCode::SC_SUCCESS, context);
        }
        else if (auto err = WSAGetLastError(); !(connectres == FALSE && err == ERROR_IO_PENDING)) {
            Context_->PendingIO -= 1;
            iodone(TranslateError(&err), context);
        }
    }

    void Socket::continue_connect(bool success, Win_IO_Connect_Context *context)
    {
        static auto iodone = [](auto code, auto context) {
            auto chandle(std::move(context->completionhandler));
            delete context;
            chandle(code);
        };
        if (success) {
            if (::setsockopt(handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) == SOCKET_ERROR) {
                iodone(TranslateError(), context);
            }
            else {
                iodone(StatusCode::SC_SUCCESS, context);
            }
        }
        else {
            iodone(TranslateError(), context);
        }
    }
    void Socket::connect(sockaddr &address, const std::function<void(StatusCode)> &&handler)
    {
        auto iofailed = [](auto code, auto context) {
            auto chandle(std::move(context->completionhandler));
            delete context;
            chandle(code);
        };
        auto context = new Win_IO_Connect_Context();
        context->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoInitConnect;
        context->Socket_ = this;
        context->address = address;
        if (Context_->inWorkerThread()) {
            init_connect(true, context);
        }
        else {
            Context_->PendingIO += 1;
            if (PostQueuedCompletionStatus(Context_->iocp.handle, 0, (ULONG_PTR)this, (LPOVERLAPPED)&context->Overlapped) == FALSE) {
                Context_->PendingIO -= 1;
                iofailed(TranslateError(), context);
            }
        }
    }
    void Socket::send(size_t buffer_size, unsigned char *buffer, const std::function<void(StatusCode, size_t)> &&handler)
    {
        assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
        WriteContext.buffer = buffer;
        WriteContext.bufferlen = buffer_size;
        WriteContext.completionhandler = std::move(handler);
        WriteContext.IOOperation = IO_OPERATION::IoWrite;
        continue_write(true);
    }
    void Socket::continue_write(bool success)
    {
        if (!success) {
            return IOComplete(this, TranslateError(), 0, &WriteContext);
        }
        else if (WriteContext.transfered_bytes == WriteContext.bufferlen) {
            return IOComplete(this, StatusCode::SC_SUCCESS, WriteContext.transfered_bytes, &WriteContext);
        }
        else {
            Context_->PendingIO += 1;
            WSABUF wsabuf;
            wsabuf.buf = (char *)WriteContext.buffer + WriteContext.transfered_bytes;
            wsabuf.len = static_cast<decltype(wsabuf.len)>(WriteContext.bufferlen - WriteContext.transfered_bytes);
            DWORD dwSendNumBytes(0), dwFlags(0);
            if (auto nRet = WSASend(handle, &wsabuf, 1, &dwSendNumBytes, dwFlags, &(WriteContext.Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                Context_->PendingIO -= 1;
                return IOComplete(this, TranslateError(&nRet), 0, &WriteContext);
            }
        }
    }

} // namespace NET
} // namespace SL
