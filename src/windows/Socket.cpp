
#include "IO_Context.h"
#include "Socket.h"
#include "common/List.h"
#include <assert.h>

namespace SL {
namespace NET {

    std::shared_ptr<ISocket> SOCKET_LITE_EXTERN CreateSocket(const std::shared_ptr<IIO_Context> &iocontext)
    {
        auto context = static_cast<IO_Context *>(iocontext.get());
        return std::make_shared<Socket>(context->PendingIO);
    }

    Socket::Socket(std::atomic<size_t> &counter) : PendingIO(counter) { handle = INVALID_SOCKET; }
    Socket::~Socket()
    { // make sure the links are all cleaned up
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
            s->close();
        }
        s->PendingIO -= 1;
        context->completionhandler(bytes);
        delete context;
    }
    void Socket::async_connect(const std::shared_ptr<IIO_Context> &io_context, std::vector<SL::NET::sockaddr> &addresses,
                               const std::function<bool(bool, SL::NET::sockaddr &)> &&handler)
    {
        if (addresses.empty())
            return;
        close();
        auto iocontext = static_cast<IO_Context *>(io_context.get());
        GUID guid = WSAID_CONNECTEX;
        DWORD bytes = 0;
        LPFN_CONNECTEX connectex;
        auto context = new Win_IO_Connect_Context();
        context->completionhandler = std::move(handler);
        context->IOOperation = IO_OPERATION::IoConnect;
        context->iocp = iocontext->iocp.handle;
        PendingIO += 1;
        continue_connect(context);
    }
    void Socket::async_read(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        if (handle == INVALID_SOCKET) {
            handler(-1);
        }
        auto ReadContext = new Win_IO_RW_Context();
        ReadContext->buffer = buffer;
        ReadContext->bufferlen = buffer_size;
        ReadContext->completionhandler = std::move(handler);
        ReadContext->IOOperation = IO_OPERATION::IoRead;
        ReadContext->wsabuf.buf = nullptr;
        ReadContext->wsabuf.len = 0;
        DWORD dwSendNumBytes(0), dwFlags(0);
        // do read with zero bytes to prevent memory from being mapped
        if (auto nRet = WSARecv(handle, &ReadContext->wsabuf, 1, &dwSendNumBytes, &dwFlags, &(ReadContext->Overlapped), NULL);
            nRet == SOCKET_ERROR && (ERROR_IO_PENDING != WSAGetLastError())) {
            return IOComplete(this, -1, ReadContext);
        }
    }

    void Socket::continue_read(Win_IO_RW_Context *sockcontext)
    {
        // read any data available
        if (sockcontext->bufferlen < sockcontext->transfered_bytes) {
            auto bytes_read = 0;
            do {
                auto remainingbytes = sockcontext->bufferlen - sockcontext->transfered_bytes;
                bytes_read = recv(handle, (char *)sockcontext->buffer + sockcontext->transfered_bytes, remainingbytes, 0);
                if (bytes_read > 0) {
                    sockcontext->transfered_bytes += bytes_read;
                }
                else if (bytes_read == 0 || (bytes_read == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
                    return IOComplete(this, -1, sockcontext);
                }
            } while (bytes_read > 0);
        }

        if (sockcontext->bufferlen == sockcontext->transfered_bytes) {
            PendingIO -= 1;
            sockcontext->completionhandler(sockcontext->transfered_bytes);
        }
        else {
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
    void Socket::continue_connect(bool connect_success, Win_IO_Connect_Context *sockcontext)
    {
        PendingIO -= 1;
        if (connect_success) {
            sockcontext->completionhandler(connect_success, sockcontext->RemainingAddresses.back());
        }
        auto addr = sockcontext->RemainingAddresses.back();
        sockcontext->RemainingAddresses.pop_back();
        if (addr.Family == Address_Family::IPV4) {
            context->ConnectSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        }
        else {
            context->ConnectSocket = WSASocketW(AF_INET6, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        }
        if (!bind(addr)) {
            close();
            PendingIO -= 1;
            context->completionhandler(false, addr);
            delete context;
        }
        if (WSAIoctl(handle, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &connectex, sizeof(connectex), &bytes, NULL, NULL) !=
            SOCKET_ERROR) {
            if (!Socket::UpdateIOCP(handle, &context->iocp, this)) {
                context->completionhandler(false, addr);
            }
        }
        else {
            std::cerr << "failed to load ConnectEX: " << WSAGetLastError() << std::endl;
            return IOComplete(this, -1, context);
        }

        if (addr.Family == Address_Family::IPV4) {
            ::sockaddr_in sockaddr = {0};
            int sockaddrlen = sizeof(sockaddr);
            inet_pton(AF_INET, addr.Address, &sockaddr.sin_addr);
            DWORD bytessend = 0;
            auto connectres = connectex(handle, (::sockaddr *)&sockaddr, sockaddrlen, NULL, 0, &bytessend, (LPOVERLAPPED)&context->Overlapped);
            if (connectres == TRUE) {
            }
            else if (connectres == FALSE && WSAGetLastError() == ERROR_IO_PENDING) {
                return;
            }
        }
        else {
            sockaddr_in6 sockaddr = {0};
            int sockaddrlen = sizeof(sockaddr);
            inet_pton(AF_INET6, addr.Address, &sockaddr.sin6_addr);
            DWORD bytessend = 0;
            auto connectres = connectex(handle, (::sockaddr *)&sockaddr, sockaddrlen, NULL, 0, &bytessend, (LPOVERLAPPED)&context->Overlapped);
            if (connectres == TRUE) {
            }
            else if (connectres == FALSE && WSAGetLastError() == ERROR_IO_PENDING) {
                return;
            }
        }
        IOComplete(this, -1, context);
    }
    void Socket::async_write(size_t buffer_size, unsigned char *buffer, const std::function<void(Bytes_Transfered)> &&handler)
    {
        auto val = new Win_IO_RW_Context();
        val->buffer = buffer;
        val->bufferlen = buffer_size;
        val->completionhandler = handler;
        val->IOOperation = IO_OPERATION::IoWrite;
        continue_write(val);
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
