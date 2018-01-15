
#include "IO_Context.h"
#include "Socket.h"
#include "common/List.h"
#include <assert.h>

namespace SL {
namespace NET {

    std::shared_ptr<ISocket> SOCKET_LITE_EXTERN CreateSocket(const std::shared_ptr<IIO_Context> &iocontext, Address_Family family)
    {
        auto context = std::static_pointer_cast<IO_Context>(iocontext);
        return std::make_shared<Socket>(context, family);
    }

    Socket::Socket(std::shared_ptr<IO_Context> &context, Address_Family family) : PendingIO(context->PendingIO), IO_Context_(context)
    {
        if (family == Address_Family::IPV4) {
            handle = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        }
        else {
            handle = WSASocketW(AF_INET6, SOCK_STREAM, IPPROTO_IP, NULL, 0, WSA_FLAG_OVERLAPPED);
        }
    }
    Socket::~Socket()
    {
        std::cout << "~Socket() " << std::endl;
        close();
    }

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

    void Socket::close()
    {
        if (handle != INVALID_SOCKET) {
            closesocket(handle);
        }
        handle = INVALID_SOCKET;
    }
    template <typename T> void IOComplete(Socket *s, Bytes_Transfered bytes, T *context)
    {
        if (bytes == -1) {
            std::cout << "Closing Socket " << std::endl;
            s->close();
            s->PendingIO -= 1;
        }

        context->completionhandler(bytes);
        context->clear();
    }

    void Socket::recv(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        if (handle == INVALID_SOCKET) {
            handler(-1);
        }
        PendingIO += 1;
        assert(ReadContext.IOOperation == IO_OPERATION::IoNone);
        ReadContext.buffer = buffer;
        ReadContext.bufferlen = buffer_size;
        ReadContext.completionhandler = std::move(handler);
        ReadContext.IOOperation = IO_OPERATION::IoRead;
        ReadContext.wsabuf.buf = nullptr;
        ReadContext.wsabuf.len = 0;
        DWORD dwSendNumBytes(0), dwFlags(0);
        // do read with zero bytes to prevent memory from being mapped
        if (auto nRet = WSARecv(handle, &ReadContext.wsabuf, 1, &dwSendNumBytes, &dwFlags, &(ReadContext.Overlapped), NULL);
            nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            return IOComplete(this, -1, &ReadContext);
        }
    }

    void Socket::continue_read(Win_IO_RW_Context *sockcontext)
    {
        // read any data available
        if (sockcontext->transfered_bytes < sockcontext->bufferlen) {

            while (true) {
                auto remainingbytes = sockcontext->bufferlen - sockcontext->transfered_bytes;
                if (auto bytes_read = ISocket::recv(remainingbytes, sockcontext->buffer + sockcontext->transfered_bytes)) {
                    if (bytes_read > 0) {
                        sockcontext->transfered_bytes += bytes_read;
                    }
                    else if (bytes_read == -1) {
                        return IOComplete(this, -1, sockcontext);
                    }
                    else {
                        break;
                    }
                }
            }
        }

        if (sockcontext->bufferlen == sockcontext->transfered_bytes) {
            return IOComplete(this, sockcontext->transfered_bytes, sockcontext);
        }
        else {
            PendingIO += 1;
            // still more data to read.. keep going!
            sockcontext->wsabuf.buf = nullptr;
            sockcontext->wsabuf.len = 0;
            DWORD dwSendNumBytes(0), dwFlags(0);
            // do read with zero bytes to prevent memory from being mapped
            if (auto nRet = WSARecv(handle, &sockcontext->wsabuf, 1, &dwSendNumBytes, &dwFlags, &(sockcontext->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                return IOComplete(this, -1, sockcontext);
            }
        }
    }

    void Socket::connect(sockaddr &address, const std::function<void(ConnectionAttemptStatus)> &&handler)
    {
        close();
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
        auto iocontext = IO_Context_.lock();
        if (!iocontext)
            return handler(ConnectionAttemptStatus::FailedConnect);

        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        if (WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &iocontext->ConnectEx_, sizeof(iocontext->ConnectEx_), &bytes,
                     NULL, NULL) != SOCKET_ERROR) {
            if (!Socket::UpdateIOCP(handle, &iocontext->iocp.handle, this)) {
                return handler(ConnectionAttemptStatus::FailedConnect);
            }
        }
        else {
            std::cerr << "failed to load ConnectEX: " << WSAGetLastError() << std::endl;
            return handler(ConnectionAttemptStatus::FailedConnect);
        }
        auto context = new Win_IO_Connect_Context();
        context->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoConnect;
        PendingIO += 1;
        DWORD bytessend = 0;
        auto connectres = iocontext->ConnectEx_(handle, (::sockaddr *)address.get_SocketAddr(), address.get_SocketAddrLen(), NULL, 0, &bytessend,
                                                (LPOVERLAPPED)&context->Overlapped);

        if (!(connectres == TRUE || (connectres == FALSE && WSAGetLastError() == ERROR_IO_PENDING))) {
            PendingIO -= 1;
            context->completionhandler(ConnectionAttemptStatus::FailedConnect);
            delete context;
        }
    }
    void Socket::continue_connect(ConnectionAttemptStatus connect_success, Win_IO_Connect_Context *sockcontext)
    {

        if (::setsockopt(handle, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0) == SOCKET_ERROR) {
            std::cerr << "Error setsockopt SO_UPDATE_CONNECT_CONTEXT Code: " << WSAGetLastError() << std::endl;
            connect_success = ConnectionAttemptStatus::FailedConnect;
        }
        sockcontext->completionhandler(connect_success);
        delete sockcontext;
    }
    void Socket::async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        if (handle == INVALID_SOCKET) {
            handler(-1);
        }
        assert(WriteContext.IOOperation == IO_OPERATION::IoNone);
        WriteContext.buffer = buffer;
        WriteContext.bufferlen = buffer_size;
        WriteContext.completionhandler = std::move(handler);
        WriteContext.IOOperation = IO_OPERATION::IoWrite;
        continue_write(&WriteContext);
    }
    void Socket::continue_write(Win_IO_RW_Context *sockcontext)
    {
        if (sockcontext->transfered_bytes == -1) {
            return IOComplete(this, -1, sockcontext);
        }
        else if (sockcontext->transfered_bytes == sockcontext->bufferlen) {
            return IOComplete(this, sockcontext->transfered_bytes, sockcontext);
        }
        else {
            sockcontext->wsabuf.buf = (char *)sockcontext->buffer + sockcontext->transfered_bytes;
            sockcontext->wsabuf.len = sockcontext->bufferlen - sockcontext->transfered_bytes;
            DWORD dwSendNumBytes(0), dwFlags(0);
            if (auto nRet = WSASend(handle, &sockcontext->wsabuf, 1, &dwSendNumBytes, dwFlags, &(sockcontext->Overlapped), NULL);
                nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
                return IOComplete(this, -1, sockcontext);
            }
        }
    }

} // namespace NET
} // namespace SL
