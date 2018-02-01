
#include "Context.h"
#include "Socket.h"
#include "common/List.h"
#include <assert.h>

namespace SL {
namespace NET {

    Socket::Socket(Context *context, Address_Family family) : Socket(context)
    {
        if (family == Address_Family::IPV4) {
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
            std::cerr << "CreateIoCompletionPort() failed: " << GetLastError() << std::endl;
            return false;
        }
    }
    template <typename T> void IOComplete(Socket *s, Bytes_Transfered bytes, T *context)
    {
        if (bytes == -1) {
            s->close();
        }
        auto handler(std::move(context->completionhandler));
        context->clear();
        handler(bytes);
    }

    void Socket::recv(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        if (handle == INVALID_SOCKET) {
            handler(-1);
        }
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
            return IOComplete(this, -1, &ReadContext);
        }
    }

    void Socket::continue_read(bool success, Win_IO_RW_Context *sockcontext)
    {
        if (!success) {
            return IOComplete(this, -1, sockcontext);
        }

        if (sockcontext->bufferlen == sockcontext->transfered_bytes) {
            return IOComplete(this, sockcontext->transfered_bytes, sockcontext);
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
                return IOComplete(this, -1, sockcontext);
            }
        }
    }

    void Socket::connect(sockaddr &address, const std::function<void(ConnectionAttemptStatus)> &&handler)
    {
        close();
        ReadContext.clear();
        if (address.get_Family() == Address_Family::IPV4) {
            sockaddr_in bindaddr = {0};
            bindaddr.sin_family = AF_INET;
            bindaddr.sin_addr.s_addr = INADDR_ANY;
            bindaddr.sin_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, Address_Family::IPV4);
            if (!bind(mytestaddr)) {
                return handler(ConnectionAttemptStatus::FailedConnect);
            }
        }
        else {
            sockaddr_in6 bindaddr = {0};
            bindaddr.sin6_family = AF_INET6;
            bindaddr.sin6_addr = in6addr_any;
            bindaddr.sin6_port = 0;
            sockaddr mytestaddr((unsigned char *)&bindaddr, sizeof(bindaddr), "", 0, Address_Family::IPV6);
            if (!bind(mytestaddr)) {
                return handler(ConnectionAttemptStatus::FailedConnect);
            }
        }

        if (!Context_->ConnectEx_) {
            GUID guid = WSAID_CONNECTEX;
            DWORD bytes = 0;
            if (WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &Context_->ConnectEx_, sizeof(Context_->ConnectEx_), &bytes,
                         NULL, NULL) == SOCKET_ERROR) {
                std::cerr << "failed to load ConnectEX: " << WSAGetLastError() << std::endl;
                return handler(ConnectionAttemptStatus::FailedConnect);
            }
        }
        if (!Socket::UpdateIOCP(handle, &Context_->iocp.handle, this)) {
            return handler(ConnectionAttemptStatus::FailedConnect);
        }

        ReadContext.completionhandler = [ihandle(std::move(handler))](Bytes_Transfered bytestransfered)
        {
            if (bytestransfered == -1) {
                ihandle(ConnectionAttemptStatus::FailedConnect);
            }
            else {
                ihandle(ConnectionAttemptStatus::SuccessfullConnect);
            }
        };
        ;
        ReadContext.IOOperation = IO_OPERATION::IoConnect;
        Context_->PendingIO += 1;
        DWORD bytessend = 0;
        auto connectres = Context_->ConnectEx_(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen(), NULL, 0, &bytessend,
                                               (LPOVERLAPPED)&ReadContext.Overlapped);

        if (!(connectres == TRUE || (connectres == FALSE && WSAGetLastError() == ERROR_IO_PENDING))) {
            Context_->PendingIO -= 1;
            auto chandle(std::move(ReadContext.completionhandler));
            chandle(-1);
        }
    }
    void Socket::continue_connect(ConnectionAttemptStatus connect_success, Win_IO_RW_Context *sockcontext)
    {
        if (connect_success == ConnectionAttemptStatus::SuccessfullConnect &&
            ::setsockopt(handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) == SOCKET_ERROR) {
            std::cerr << "Error setsockopt SO_UPDATE_CONNECT_CONTEXT Code: " << WSAGetLastError() << std::endl;
            connect_success = ConnectionAttemptStatus::FailedConnect;
        }

        auto chandle(std::move(sockcontext->completionhandler));
        if (connect_success == ConnectionAttemptStatus::FailedConnect) {
            chandle(-1);
        }
        else {
            chandle(1);
        }
        sockcontext->clear();
    }
    void Socket::send(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        if (handle == INVALID_SOCKET) {
            handler(-1);
        }
        assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
        WriteContext.buffer = buffer;
        WriteContext.bufferlen = buffer_size;
        WriteContext.completionhandler = std::move(handler);
        WriteContext.IOOperation = IO_OPERATION::IoWrite;
        continue_write(true, &WriteContext);
    }
    void Socket::continue_write(bool success, Win_IO_RW_Context *sockcontext)
    {
        if (sockcontext->transfered_bytes == -1 || !success) {
            return IOComplete(this, -1, sockcontext);
        }
        else if (sockcontext->transfered_bytes == sockcontext->bufferlen) {
            return IOComplete(this, sockcontext->transfered_bytes, sockcontext);
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
                return IOComplete(this, -1, sockcontext);
            }
        }
    }

} // namespace NET
} // namespace SL
